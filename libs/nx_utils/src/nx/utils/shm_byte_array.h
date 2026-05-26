// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace nx::utils {

class ShmByteArray;

namespace detail {

NX_UTILS_API ShmByteArray makeShmByteArray(
    void* data,
    std::size_t size,
    std::uintptr_t handle,
    std::string segmentName,
    std::shared_ptr<void> lifetimeGuard);

} // namespace detail

/**
 * Byte range backed by shared memory.
 *
 * The payload is typically written once by a producer and then treated as immutable by consumers.
 * Instances are created by shared-memory allocation helpers with a lifetime guard that keeps the
 * mapped/allocation memory valid while this object exists.
 */
class ShmByteArray
{
public:
    const char* constData() const { return m_data; }
    const char* data() const { return m_data; }
    char* data() { return m_data; }

    std::size_t size() const { return m_size; }

    std::uintptr_t handle() const { return m_handle; }
    std::string_view segmentName() const { return m_segmentName; }

private:
    friend ShmByteArray detail::makeShmByteArray(
        void*,
        std::size_t,
        std::uintptr_t,
        std::string,
        std::shared_ptr<void>);

    ShmByteArray(
        void* data,
        std::size_t size,
        std::uintptr_t handle,
        std::string segmentName,
        std::shared_ptr<void> lifetimeGuard)
        :
        m_data(static_cast<char*>(data)),
        m_size(size),
        m_handle(handle),
        m_segmentName(std::move(segmentName)),
        m_lifetimeGuard(std::move(lifetimeGuard))
    {
    }

    char* m_data = nullptr;
    std::size_t m_size = 0;
    std::uintptr_t m_handle = 0;
    std::string m_segmentName;
    std::shared_ptr<void> m_lifetimeGuard;
};

} // namespace nx::utils
