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
#include <iostream>
#include <exception>

#include "drob_internal.h"
#include "Rewriter.hpp"
#include "Registry.hpp"
#include "arch.hpp"

namespace drob {

drob_loglevel loglevel = DROB_LOGLEVEL_NONE;
FILE *logfile = stdout;

int drobcpp_setup(void)
{
    return arch_setup();
}

void drobcpp_teardown(void)
{
    Registry::instance().deleteAllFunctions();

    arch_teardown();
}

const uint8_t *drobcpp_optimize(const uint8_t * itext, const drob_cfg *cfg)
{
    drob_info("Optimizing function: %p", itext);

    if (!itext) {
        drob_error("No function specified");
        goto error;
    } else if (!cfg) {
        drob_error("No configuration specified");
        goto error;
    }

    try {
        Rewriter rewriter(itext, cfg);
        std::unique_ptr<BinaryPool> rewritten(rewriter.rewrite());

        if (rewritten) {
            const uint8_t *entry = rewritten->getEntry();

            drob_info("Generated code size: %u bytes",
                      rewritten->getCodeSize());
            drob_info("Used constant pool size: %u bytes",
                      rewritten->getConstantPoolSize());

            Registry::instance().addFunction(entry, std::move(rewritten));
            return entry;
        }
        drob_error("Unknown error generating code");
    } catch (std::exception &e) {
        drob_error(e.what());
    }

error:
    switch(cfg->error_handling) {
    case DROB_ERROR_HANDLING_RETURN_NULL:
        return nullptr;
    case DROB_ERROR_HANDLING_RETURN_ORIGINAL:
        return itext;
    case DROB_ERROR_HANDLING_ABORT:
    default:
        abort();
    }
}

void drobcpp_free(const uint8_t *itext)
{
    Registry::instance().deleteFunction(itext);
}

} /* namespace drob */
