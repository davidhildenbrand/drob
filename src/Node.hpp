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
#ifndef NODE_HPP
#define NODE_HPP

namespace drob {

class Node {
public:
    Node(Node *parent) : parent(parent) {}

    bool visited{false};
    bool livenessAnalysisValid{false};
    bool stackAnalysisValid{false};
    bool queued{false};

    void invalidateStackAnalysis(void)
    {
        if (!stackAnalysisValid)
            return;
        stackAnalysisValid = false;
        if (parent)
            parent->invalidateStackAnalysis();
    }
    void invalidateLivenessAnalysis(void)
    {
        if (!livenessAnalysisValid)
            return;
        livenessAnalysisValid = false;
        if (parent)
            parent->invalidateLivenessAnalysis();
    }
private:
    Node *parent;
};

} /* namespace drob */

#endif /* NODE_HPP */
