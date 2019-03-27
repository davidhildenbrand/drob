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
#ifndef NODE_CALLBACK_HPP
#define NODE_CALLBACK_HPP

namespace drob {

class Instruction;
class SuperBlock;
class Function;

class NodeCallback {
public:
    NodeCallback() = default;

    virtual int handleInstruction(Instruction *instruction,
                      SuperBlock *block, Function *function)
    {
        (void)instruction;
        (void)block;
        (void)function;
        drob_assert_not_reached();
        return -EINVAL;
    }

    virtual int handleBlock(SuperBlock *block, Function *function)
    {
        (void)block;
        (void)function;
        drob_assert_not_reached();
        return -EINVAL;
    }

    virtual int handleFunction(Function *function)
    {
        (void)function;
        drob_assert_not_reached();
        return -EINVAL;
    }
};

} /* namespace drob */

#endif /* NODE_CALLBACK_HPP */
