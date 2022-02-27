// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fallback_shared_memory.h"

namespace nx::vms::client::desktop {

bool FallbackSharedMemory::initialize()
{
    return true;
}

void FallbackSharedMemory::lock()
{
}

void FallbackSharedMemory::unlock()
{
}

SharedMemoryData FallbackSharedMemory::data() const
{
    return m_data;
}

void FallbackSharedMemory::setData(const SharedMemoryData& data)
{
    m_data = data;
}

} // namespace nx::vms::client::desktop
