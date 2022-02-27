// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scoped_garbage_collector.h"

namespace nx::utils {

ScopedGarbageCollector::~ScopedGarbageCollector()
{
    destroyAll();
}

void ScopedGarbageCollector::destroyAll()
{
    while (!m_ptrs.empty())
        m_ptrs.pop_back();

    m_ptrs.shrink_to_fit();
}

} // namespace nx::utils
