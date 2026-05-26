// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shm_byte_array.h"

#include <nx/utils/log/assert.h>

namespace nx::utils::detail {

ShmByteArray makeShmByteArray(
    void* data,
    std::size_t size,
    std::uintptr_t handle,
    std::string segmentName,
    std::shared_ptr<void> lifetimeGuard)
{
    NX_ASSERT(data && size > 0, "ShmByteArray requires a non-empty shared-memory range");
    return ShmByteArray(data, size, handle, std::move(segmentName), std::move(lifetimeGuard));
}

} // namespace nx::utils::detail
