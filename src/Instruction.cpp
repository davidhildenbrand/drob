/*
 * This file is part of Drob.
 *
 * Copyright 2019 David Hildenbrand <davidhildenbrand@gmail.com>
 *
 * Drob is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Drob is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * in the COPYING.LESSER files in the top-level directory for more details.
 */
#include "Instruction.hpp"
#include "BinaryPool.hpp"
#include "Rewriter.hpp"

using namespace drob;

/*
 * Whenever we have a memory operand, the registers used to construct
 * the indirect memory access are actually registers read by the instruction.
 */
static void addAddrRegs(SubRegisterMask &mask, const StaticMemPtr &ptr)
{
    if (ptr.type != MemPtrType::SIB) {
        return;
    }
    if (ptr.sib.base != Register::None) {
        mask += getSubRegisterMask(ptr.sib.base);
    }
    if (ptr.sib.index != Register::None) {
        mask += getSubRegisterMask(ptr.sib.index);
    }
}

/*
 * Merge operands with explicit operand info to create a raw operand
 * info (which looks than equal to an implicit operand info).
 */
static StaticOperandInfo createRawOperandInfo(const ExplicitStaticOperandInfo &explInfo,
                                           const StaticOperand &operand)
{
    StaticOperandInfo rawInfo;

    rawInfo.type = explInfo.type;
    switch (explInfo.type) {
    case OperandType::Register:
        rawInfo.r.reg = operand.reg;
        rawInfo.r.mode = explInfo.r.mode;
        rawInfo.r.r = explInfo.r.r;
        rawInfo.r.w = explInfo.r.w;
        break;
    case OperandType::MemPtr:
        rawInfo.m.ptr = operand.mem;
        rawInfo.m.mode = explInfo.m.mode;
        rawInfo.m.size = explInfo.m.size;
        break;
    default:
        /* immediate */
        rawInfo.imm = operand.imm;
    }

    return rawInfo;
}

OperandInfo Instruction::createOperandInfo(const StaticOperandInfo &rawInfo,
                                           int nr, bool isImpl) const
{
    OperandInfo info;

    info.type = rawInfo.type;
    info.nr = nr;
    info.isImpl = isImpl;
    switch (info.type) {
    case OperandType::Register:
        info.r = rawInfo.r;
        break;
    case OperandType::MemPtr:
        info.m = rawInfo.m;
        break;
    default:
        /* immediate */
        info.imm = rawInfo.imm;
    }

    /* For now, always try to refine when we have a handler */
    if (unlikely(opcodeInfo && opcodeInfo->refine)) {
        opcodeInfo->refine(info, opcode, rawOperands, nullptr);
    }
    return info;
}

static void addOperandToRegisterMasks(InstructionInfo &info,
                                      const OperandInfo &opInfo)
{
    switch (opInfo.type) {
    case OperandType::MemPtr:
        if (opInfo.m.mode != AccessMode::None) {
            addAddrRegs(info.readRegs, opInfo.m.ptr);
        }
        if (isWrite(opInfo.m.mode)) {
            info.mayWriteMem = true;
        }
        break;
    case OperandType::Register:
        if (isRead(opInfo.r.mode)) {
            info.readRegs += getSubRegisterMask(opInfo.r.reg, opInfo.r.r);
        }
        if (isWrite(opInfo.r.mode)) {
            if (isConditional(opInfo.r.mode)) {
                info.condWrittenRegs += getSubRegisterMask(opInfo.r.reg,
                                                           opInfo.r.w);
            } else {
                info.writtenRegs += getSubRegisterMask(opInfo.r.reg,
                                                       opInfo.r.w);
            }
        }
        break;
    default:
        break;
    }
}

/*
 * Return information about this instruction which registers are
 * read/written.
 *
 * Caller has to take care of (in addition to this):
 * - RET will have to read what we are returning from the function
 * - CALL will have to read what the function expects as input and write
 *   what the function outputs.
 * - JMP will have to read what the target expects.
 * -> Unknown CALL/JMP targets can read/write anything!
 */
const InstructionInfo& Instruction::getInfo(void) const
{
    int i;

    if (cachedInfo) {
        return *cachedInfo.get();
    }

    /* without opcode info, we have to assume everything is possible */
    if (!opcodeInfo) {
        static InstructionInfo unknown;
        static bool initialized;

        if (!initialized) {
            unknown.nasty = true;
            /* nasty instructions read/write any registers */
            unknown.mayWriteMem = false;
            unknown.readRegs.fill();
            unknown.writtenRegs.fill();
            initialized = true;
        }
        return unknown;
    }

    cachedInfo = std::make_unique<InstructionInfo>();
    cachedInfo->nasty = false;
    cachedInfo->mayWriteMem = false;
    cachedInfo->predicateRegs = {};
    cachedInfo->readRegs = {};
    cachedInfo->writtenRegs = {};
    cachedInfo->condWrittenRegs = {};

    /* process the explicit operands */
    for (i = 0; i < opcodeInfo->numOperands; i++) {
        drob_assert(opcodeInfo->opInfo);

        StaticOperandInfo rawInfo = createRawOperandInfo(opcodeInfo->opInfo[i],
                                                      rawOperands.op[i]);
        OperandInfo opInfo = createOperandInfo(rawInfo, i, false);

        addOperandToRegisterMasks(*cachedInfo, opInfo);
        cachedInfo->operands.push_back(opInfo);
    }

    /* process the implicit operands */
    for (i = 0; i < opcodeInfo->numImplOperands; i++) {
        drob_assert(opcodeInfo->iOpInfo);
        OperandInfo opInfo = createOperandInfo(opcodeInfo->iOpInfo[i],
                                               i + opcodeInfo->numOperands, true);

        addOperandToRegisterMasks(*cachedInfo, opInfo);
        cachedInfo->operands.push_back(opInfo);
    }


    /* Generate the predicate register mask */
    if (opcodeInfo->predicate) {
        for (int i = 0; i < 2; i++) {
            const PredComparison &comp = opcodeInfo->predicate->comparisons[i];

            if (!comp.lhs.isImm) {
                cachedInfo->predicateRegs += getSubRegisterMask(comp.lhs.reg);
            }
            if (!comp.rhs.isImm) {
                cachedInfo->predicateRegs += getSubRegisterMask(comp.rhs.reg);
            }
            if (comp.con == PredConjunction::None) {
                break;
            }
        }
    }

    return *cachedInfo.get();
}

static DynamicValue calculateMemPtr(const MemPtr& ptr)
{
    drob_assert(ptr.type == MemPtrType::SIB);

    /* fast path */
    if (ptr.sib.base.isImm() && ptr.sib.index.isImm() && ptr.sib.disp.isImm()) {
        return DynamicValue(ptr.sib.base.getImm64() +
                            ptr.sib.scale * ptr.sib.index.getImm64() +
                            ptr.sib.disp.getImm64());
    }

    /* base + scale * index + disp */
    DynamicValue tmp1 = multiplyDynamicValue(ptr.sib.index, ptr.sib.scale);
    DynamicValue tmp2 = addDynamicValues(ptr.sib.base, tmp1);
    return addDynamicValues(ptr.sib.disp, tmp2);
}

/* Read data via a pointer */
static DynamicValue emulateReadAccess(const MemAccess &memAcc, const ProgramState &ps,
                                      const RewriterCfg &cfg,
                                      const MemProtCache &memProtCache)
{
    bool isConst = false;
    uint8_t *ptr;

    if (memAcc.ptrVal.isTainted())
        return DynamicValue(DynamicValueType::Tainted);
    /*
     * Reading from unknown locations always results in Unknown. This is fine,
     * as we stop stack analysis as soon as a tainted value or a stack pointer
     * is written to an untracked location.
     */
    if (memAcc.ptrVal.isUnknownOrDead())
        return DynamicValue(DynamicValueType::Unknown);
    if (memAcc.ptrVal.isReturnPtr())
        drob_throw("Trying to read via the return pointer.");
    if (memAcc.ptrVal.isStackPtr())
        return ps.getStack(memAcc.ptrVal.getPtrOffset(), memAcc.size);
    if (memAcc.ptrVal.isUsrPtr()) {
        const UsrPtrCfg& ptrCfg = cfg.getUsrPtrCfg(memAcc.ptrVal.getNr());

        if (!ptrCfg.isKnown)
            return DynamicValue(DynamicValueType::Unknown);
        if (ptrCfg.isConst)
            isConst = true;
        ptr = (uint8_t *)ptrCfg.val + memAcc.ptrVal.getPtrOffset();
    } else {
        drob_assert(memAcc.ptrVal.isImm());
        ptr = (uint8_t *)memAcc.ptrVal.getImm64();
    }
    /*
     * At this point we are reading from a known host address. Let's see
     * if we are reading constant data.
     */
    if (!isConst && !memProtCache.isConstant((uint64_t)ptr, static_cast<uint8_t>(memAcc.size)))
        return DynamicValue(DynamicValueType::Unknown);

    switch (memAcc.size) {
    case MemAccessSize::B1:
        return DynamicValue(*ptr);
    case MemAccessSize::B2:
        return DynamicValue (*((uint16_t*)ptr));
    case MemAccessSize::B4:
        return DynamicValue (*((uint32_t*)ptr));
    case MemAccessSize::B8:
        return DynamicValue (*((uint64_t*)ptr));
    case MemAccessSize::B16:
        return DynamicValue (*((__uint128_t*)ptr));
    default:
        drob_throw("Unsupported memory access size detected");
    }
}

static MemAccess createMemAccess(const StaticMemAccess& rawMemAccess,
                                 const ProgramState &ps)
{
    MemAccess memAccess = {};

    memAccess.ptr.type = rawMemAccess.ptr.type;
    memAccess.size = rawMemAccess.size;
    memAccess.mode = rawMemAccess.mode;

    switch (rawMemAccess.ptr.type) {
    case MemPtrType::Direct:
        memAccess.ptr.addr = rawMemAccess.ptr.addr;
        if (rawMemAccess.ptr.addr.usrPtrNr < 0) {
            memAccess.ptrVal = DynamicValue(rawMemAccess.ptr.addr.val);
        } else {
            memAccess.ptrVal = DynamicValue(DynamicValueType::UsrPtr,
                                            rawMemAccess.ptr.addr.usrPtrNr,
                                            rawMemAccess.ptr.addr.usrPtrOffset);
        }
        break;
    case MemPtrType::SIB:
        if (rawMemAccess.ptr.sib.disp.usrPtrNr < 0) {
            memAccess.ptr.sib.disp = DynamicValue((uint64_t)(int64_t)rawMemAccess.ptr.sib.disp.val);
        } else {
            memAccess.ptr.sib.disp = DynamicValue(DynamicValueType::UsrPtr,
                                                  rawMemAccess.ptr.sib.disp.usrPtrNr,
                                                  rawMemAccess.ptr.sib.disp.usrPtrOffset);
        }
        memAccess.ptr.sib.scale = rawMemAccess.ptr.sib.scale;
        if (rawMemAccess.ptr.sib.base != Register::None) {
            memAccess.ptr.sib.base = ps.getRegister(rawMemAccess.ptr.sib.base);
        } else {
            memAccess.ptr.sib.base = DynamicValue((uint64_t)0);
        }
        if (rawMemAccess.ptr.sib.index != Register::None) {
            memAccess.ptr.sib.index = ps.getRegister(rawMemAccess.ptr.sib.index);
        } else {
            memAccess.ptr.sib.index = DynamicValue((uint64_t)0);
        }
        memAccess.ptrVal = calculateMemPtr(memAccess.ptr);
        break;
    default:
        drob_assert_not_reached();
    }
    return memAccess;
}

DynamicOperandInfo Instruction::createDynOpInfo(const OperandInfo &operand,
                                                const ProgramState &ps,
                                                const RewriterCfg &cfg,
                                                const MemProtCache &memProtCache) const
{
    DynamicOperandInfo dynOperand(operand);

    switch (operand.type) {
    case OperandType::Register:
        if (unlikely(opcodeInfo && opcodeInfo->refine)) {
            OperandInfo copy = operand;

            opcodeInfo->refine(copy, opcode, rawOperands, &ps);
            dynOperand.regAcc = copy.r;
        } else {
            dynOperand.regAcc = operand.r;
        }
        if (isRead(dynOperand.regAcc.mode)) {
            dynOperand.input = ps.getRegister(dynOperand.regAcc.reg,
                                             dynOperand.regAcc.r);
            dynOperand.isInput = true;
            if (isConditional(dynOperand.regAcc.mode)) {
                dynOperand.isCondInput = true;
            }
        }
        if (isWrite(dynOperand.regAcc.mode)) {
            dynOperand.isOutput = true;
            if (isConditional(dynOperand.regAcc.mode)) {
                dynOperand.isCondOutput = true;
            }
        }
        break;
    case OperandType::MemPtr:
        if (unlikely(opcodeInfo && opcodeInfo->refine)) {
            OperandInfo copy = operand;

            opcodeInfo->refine(copy, opcode, rawOperands, &ps);
            dynOperand.memAcc = createMemAccess(copy.m, ps);
        } else {
            dynOperand.memAcc = createMemAccess(operand.m, ps);
        }

        if (isRead(dynOperand.memAcc.mode)) {
            dynOperand.input = emulateReadAccess(dynOperand.memAcc, ps, cfg, memProtCache);
            dynOperand.isInput = true;
            if (isConditional(dynOperand.memAcc.mode)) {
                dynOperand.isCondInput = true;
            }
        }
        if (isSpecial(dynOperand.memAcc.mode)) {
            dynOperand.input = dynOperand.memAcc.ptrVal;
            dynOperand.isInput = true;
            if (isConditional(dynOperand.memAcc.mode)) {
                dynOperand.isCondInput = true;
            }
        }
        if (isWrite(dynOperand.memAcc.mode)) {
            dynOperand.isOutput = true;
            if (isConditional(dynOperand.memAcc.mode)) {
                dynOperand.isCondOutput = true;
            }
        }
        break;
    default:
        /* immediate - reconstruct the usr ptr properly */
        if (operand.imm.usrPtrNr < 0) {
            dynOperand.input = DynamicValue(operand.imm.val);
        } else {
            dynOperand.input = DynamicValue(DynamicValueType::UsrPtr,
                                            operand.imm.usrPtrNr,
                                            operand.imm.usrPtrOffset);
        }
        dynOperand.isInput = true;
    }

    return dynOperand;
}

const DynamicInstructionInfo *Instruction::getDynInfo(void) const
{
    if (cachedDynInfo)
        return cachedDynInfo.get();
    return nullptr;
}

std::unique_ptr<DynamicInstructionInfo>
Instruction::genDynInfo(ProgramState &ps, const RewriterCfg &cfg,
                        const MemProtCache &memProtCache) const
{
    auto dynInfo = std::make_unique<DynamicInstructionInfo>();
    auto& info = getInfo();

    /*
     * If we are not sure that the instruction will actually execute,
     * we have to treat all writes as conditional writes - some values
     * might remain untouched. E.g. if we are not sure if a conditional
     * move will be executed or not, we have to also respect the old
     * value (and merge the new and old value). We could split it up
     * into two paths by adding an explicit check. This would allow
     * to eventually optimize both paths later.
     */
    dynInfo->opcode = opcode,
    dynInfo->willExecute = willExecute(ps);
    dynInfo->nasty = info.nasty;
    dynInfo->numInput = 0;
    dynInfo->numInputEncodedImm = 0;
    dynInfo->numInputImm = 0;
    dynInfo->numInputPtr = 0;
    dynInfo->numInputStackPtr = 0;
    dynInfo->numInputTainted = 0;
    dynInfo->mayWriteMem = false;
    dynInfo->predicateRegs = info.predicateRegs;
    dynInfo->readRegs = {};
    dynInfo->writtenRegs = {};
    dynInfo->condWrittenRegs = {};

    /* nasty instructions read/write any registers */
    if (dynInfo->nasty) {
        dynInfo->readRegs.fill();
        dynInfo->writtenRegs.fill();
    }

    /* Convert all operands using runtime information */
    for (auto &operand : info.operands) {
        DynamicOperandInfo dynOpInfo = createDynOpInfo(operand, ps, cfg,
                                                       memProtCache);

        /* count the beans to make emulation decisions easier */
        if (dynOpInfo.isInput) {
            dynInfo->numInput++;
            if (dynOpInfo.input.isImm()) {
                dynInfo->numInputImm++;
                if (dynOpInfo.type != OperandType::Register &&
                    dynOpInfo.type != OperandType::MemPtr) {
                    dynInfo->numInputEncodedImm++;
                }
            } else if (dynOpInfo.input.isTainted()) {
                dynInfo->numInputTainted++;
            } else if (dynOpInfo.input.isPtr()) {
                dynInfo->numInputPtr++;
                if (dynOpInfo.input.isStackPtr()) {
                    dynInfo->numInputStackPtr++;
                }
            }
        }

        /* prepare helpful data for register liveness analysis */
        if (dynOpInfo.isInput) {
            if (dynOpInfo.type == OperandType::Register) {
                dynInfo->readRegs += getSubRegisterMask(dynOpInfo.regAcc.reg,
                                                        dynOpInfo.regAcc.r);
            } else if (dynOpInfo.type == OperandType::MemPtr) {
                addAddrRegs(dynInfo->readRegs, operand.m.ptr);
            }
        }
        if (dynOpInfo.isOutput) {
            if (dynOpInfo.type == OperandType::Register) {
                if (dynOpInfo.isCondOutput) {
                    dynInfo->condWrittenRegs += getSubRegisterMask(dynOpInfo.regAcc.reg,
                                                                   dynOpInfo.regAcc.w);
                } else {
                    dynInfo->writtenRegs += getSubRegisterMask(dynOpInfo.regAcc.reg,
                                                               dynOpInfo.regAcc.w);
                }
            } else if (dynOpInfo.type == OperandType::MemPtr) {
                addAddrRegs(dynInfo->readRegs, operand.m.ptr);
                dynInfo->mayWriteMem = true;
            }
        }

        dynInfo->operands.push_back(dynOpInfo);
    }

    return dynInfo;
}

static void performDirectMove(DynamicOperandInfo &in, DynamicOperandInfo &out,
                              ProgramState &ps, bool cond)
{
    /*
     * On conditional moves, the simple move already merged the old and new
     * data. Merging again does not make too much sense. If we ever need
     * more details for this case, we have to hinder the first move from
     * happening.
     */
    if (cond) {
        return;
    }

    if (in.type == OperandType::Register) {
        if (out.type == OperandType::Register) {
            ps.moveRegisterRegister(in.regAcc.reg, in.regAcc.r,
                                    out.regAcc.reg, out.regAcc.w);
        } else {
            drob_assert(out.type == OperandType::MemPtr);
            if (out.memAcc.ptrVal.isStackPtr()) {
                ps.moveRegisterStack(in.regAcc.reg, in.regAcc.r,
                                     out.memAcc.ptrVal.getPtrOffset(),
                                     out.memAcc.size);
            }
            // handled by simple move
        }
    } else if (in.type == OperandType::MemPtr) {
        if (out.type == OperandType::Register) {
            if  (in.memAcc.ptrVal.isStackPtr()) {
                ps.moveStackRegister(in.memAcc.ptrVal.getPtrOffset(), in.memAcc.size,
                                     out.regAcc.reg, out.regAcc.w);
            }
            // handled by simple move
        } else {
            drob_assert(out.type == OperandType::MemPtr);

            if (in.memAcc.ptrVal.isStackPtr() && out.memAcc.ptrVal.isStackPtr()) {
                drob_assert(in.memAcc.size == out.memAcc.size);
                ps.moveStackStack(in.memAcc.ptrVal.getPtrOffset(),
                                  out.memAcc.ptrVal.getPtrOffset(),
                                  out.memAcc.size);
            }
            // handled by simple move
        }
    }
    // handled by simple move
}

static EmuRet emulateGeneric(DynamicInstructionInfo &dynInfo)
{
    DynamicValue data(DynamicValueType::Unknown);

    /* if one input is tainted or a stack pointer, everything is tainted */
    if (dynInfo.numInputTainted || dynInfo.numInputStackPtr) {
        data = DynamicValue(DynamicValueType::Tainted);
    }

    for (auto &op : dynInfo.operands) {
        if (!op.isOutput)
            continue;
        op.output = data;
    }
    return EmuRet::Ok;
}

void Instruction::emulate(ProgramState &ps, const RewriterCfg &cfg,
                          const MemProtCache &memProtCache,
                          bool cacheDynInfo) const
{
    auto dynInfo = genDynInfo(ps, cfg, memProtCache);
    EmuRet ret;

    /* No need to worry about instructions that won't be executed */
    if (dynInfo->willExecute == TriState::False) {
        goto skip_emulation;
    }

    /*
     * TODO: If we have previous data and the inputs match, we can skip the
     * actual emulation. But not sure if the comparisons are faster than the
     * actual emulator.
     */

    /*
     * Caller has to handle effects of ret/call/jmp.
     */
    if (dynInfo->nasty) {
        ps.nastyInstruction();
    } else  {
        /*
         * Run the opcode specific emulation function if possible. Fallback to
         * the generic emulator.
         */
        if (!opcodeInfo->emulate) {
            ret = emulateGeneric(*dynInfo);
        } else if (opcodeInfo->flags & OfEmuFull) {
            ret = opcodeInfo->emulate(*dynInfo, cfg);
        } else if (opcodeInfo->flags & OfEmuPtr) {
            if (dynInfo->numInput == dynInfo->numInputImm + dynInfo->numInputPtr) {
                ret = opcodeInfo->emulate(*dynInfo, cfg);
            } else {
                ret = emulateGeneric(*dynInfo);
            }
        } else {
            if (dynInfo->numInput == dynInfo->numInputImm) {
                ret = opcodeInfo->emulate(*dynInfo, cfg);
            } else {
                ret = emulateGeneric(*dynInfo);
            }
        }

        /*
         * Move all output to the destination. We have to take care of
         * conditional execution and conditional writes.
         */
        for (auto &op : dynInfo->operands) {
            bool cond = op.isCondOutput || dynInfo->willExecute == TriState::Unknown;

            if (!op.isOutput)
                continue;

            if (op.type == OperandType::Register) {
                ps.setRegister(op.regAcc.reg, op.regAcc.w, op.output, cond);
                continue;
            }

            drob_assert(op.type == OperandType::MemPtr);
            if (op.memAcc.ptrVal.isStackPtr()) {
                ps.setStack(op.memAcc.ptrVal.getPtrOffset(), op.memAcc.size,
                            op.output, cond);
            } else if (op.memAcc.ptrVal.isTainted()) {
                ps.untrackedStackAccess();
            } else if (op.output.isStackPtr() || op.output.isTainted()) {
                /*
                 * If a potential stack pointer is written to an untracked
                 * location, abort stack analysis. We could read and reuse
                 * that pointer any time without realizing it.
                 */
                ps.untrackedStackAccess();
            }
        }

        /*
         * If we had a move, move data again, but this time directly, so we get
         * more detailed results when moving mixed data. We assume here,
         * that we don't have a move where src and dst overlap.
         */
         switch (ret) {
         case EmuRet::Mov10: {
             bool cond = dynInfo->operands[0].isCondOutput ||
                         dynInfo->willExecute == TriState::Unknown;

             performDirectMove(dynInfo->operands[1], dynInfo->operands[0], ps,
                               cond);
             break;
         }
         case EmuRet::Mov02: {
             bool cond = dynInfo->operands[2].isCondOutput ||
                         dynInfo->willExecute == TriState::Unknown;

             performDirectMove(dynInfo->operands[0], dynInfo->operands[2], ps,
                               cond);
             break;
         }
         case EmuRet::Mov20: {
             bool cond = dynInfo->operands[0].isCondOutput ||
                         dynInfo->willExecute == TriState::Unknown;

             performDirectMove(dynInfo->operands[2], dynInfo->operands[0], ps,
                               cond);
             break;
         }
         default:
             break;
         }
    }

skip_emulation:
    /* If requested, cache the new value. If not, keep the previous values */
    if (cacheDynInfo) {
        this->cachedDynInfo = std::move(dynInfo);
    }
}

TriState Instruction::willExecute(const ProgramState &ps) const
{
    PredConjunction lastCon = PredConjunction::None;
    TriState result = TriState::Unknown;

    if (!opcodeInfo)
        return TriState::Unknown;
    else if (!opcodeInfo->predicate)
        return TriState::True;

    for (int i = 0;; i++) {
        const PredComparison &comp = opcodeInfo->predicate->comparisons[i];
        TriState tmp = TriState::Unknown;
        uint64_t lhs = 0, rhs = 0;
        bool bothSidesKnown = true;

        if (comp.lhs.isImm) {
            lhs = comp.lhs.imm;
        } else {
            DynamicValue data = ps.getRegister(comp.lhs.reg);

            if (data.isImm())
                lhs = data.getImm64();
            /* FIXME: known pointers */
            else
                bothSidesKnown = false;
        }

        if (comp.rhs.isImm) {
            rhs = comp.rhs.imm;
        } else {
            DynamicValue data = ps.getRegister(comp.rhs.reg);

            if (data.isImm())
                rhs = data.getImm64();
            /* FIXME: known pointers */
            else
                bothSidesKnown = false;
        }

        if (bothSidesKnown) {
            tmp = TriState::False;
            switch (comp.comp) {
            case PredComparator::Equal:
                if (lhs == rhs)
                    tmp = TriState::True;
                break;
            case PredComparator::NotEqual:
                if (lhs != rhs)
                    tmp = TriState::True;
                break;
            default:
                drob_assert_not_reached();
            }
        }

        /* Combine the results */
        switch (lastCon) {
        case PredConjunction::None:
            result = tmp;
            break;
        case PredConjunction::And:
            result &= tmp;
            break;
        case PredConjunction::Or:
            result |= tmp;
            break;
        default:
            drob_assert_not_reached();
        }

        lastCon = comp.con;
        if (lastCon == PredConjunction::None)
            break;
    }

    return result;
}

const uint8_t *Instruction::generateCode(BinaryPool &binaryPool, bool write)
{
    if (reencode) {
        if (likely(opcodeInfo && opcodeInfo->encode)) {
            /* Generate fresh code */
            uint8_t buf[ARCH_MAX_ILEN];
            int newIlen = this->opcodeInfo->encode(opcode, rawOperands, buf,
                                                (uint64_t)binaryPool.nextCode());

            uint8_t *newItext = binaryPool.allocCode(newIlen);
            if (write) {
                memcpy(newItext, buf, newIlen);
                if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG)) {
                    if (itext) {
                        drob_debug("Original instruction:");
                        arch_decode_dump(itext, itext + ilen);
                        drob_debug("Re-encoded instruction:");
                        arch_decode_dump(buf, buf + newIlen);
                    } else {
                        drob_debug("Encode new instruction:");
                        arch_decode_dump(buf, buf + newIlen);
                    }
                }
            }
            return newItext;
        } else {
            /* Modified an instruction we can't encode */
            drob_warn("Can't encode unknown instruction");
        }
    } else {
        /* copy the original instruction text */
        uint8_t *newItext = binaryPool.allocCode(ilen);

        drob_assert(itext);
        if (write) {
            if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG)) {
                drob_debug("Reusing original instruction:");
                arch_decode_dump(itext, itext + ilen);
            }
            memcpy(newItext, itext, ilen);
        }
    }
    return itext;
}

void Instruction::dump() const
{
    drob_dump("Instruction %p (%p)", this, itext);
    if (itext) {
        arch_decode_dump(this->itext, this->itext + this->ilen);
    } else {
        drob_dump("    -> No original instruction available");
    }
}
