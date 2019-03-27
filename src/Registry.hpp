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
#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <unordered_map>
#include <memory>
#include "BinaryPool.hpp"
#include "MemProtCache.hpp"

namespace drob {

class Registry {
public:
    static Registry &instance()
    {
        static Registry _instance;

        return _instance;
    }

    void addFunction(const uint8_t *itext, std::unique_ptr<BinaryPool> instance)
    {
        instances.insert(std::make_pair(itext, std::move(instance)));
    }

    void deleteFunction(const uint8_t *itext)
    {
        instances.erase(itext);
    }

    void deleteAllFunctions()
    {
        instances.clear();
    }

private:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry &operator=(const Registry &) = delete;

    std::unordered_map<const uint8_t *, std::unique_ptr<BinaryPool>> instances;

};

} /* namespace drob */

#endif /* REGISTRY_HPP */
