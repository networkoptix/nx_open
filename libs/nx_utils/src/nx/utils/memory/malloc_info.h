// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>
#include <optional>

#include <nx/reflect/instrument.h>

namespace nx::utils::memory {

/**
 * MallocInfo is the in memory representation of the malloc_info() call in linux systems.
 */
struct NX_UTILS_API MallocInfo
{
    struct MemoryStat
    {
        std::string type;
        std::optional<size_t> count;
        std::size_t size = 0;

        bool operator==(const MemoryStat&) const = default;
    };

    // <heap nr="1"> ... </heap>
    struct Heap
    {
        //<sizes>...</sizes>
        struct Sizes
        {
            // <size from="17" to="32" total="704" count="22"/>
            struct Size
            {
                std::size_t from = 0;
                std::size_t to = 0;
                std::size_t total = 0;
                std::size_t count = 0;

                bool operator==(const Size&) const = default;
            };

            std::vector<Size> size;
            std::optional<Size> unsorted;
        };

        std::size_t nr;
        Sizes sizes;
        std::vector<MemoryStat> total;
        std::vector<MemoryStat> system;
        std::vector<MemoryStat> aspace;
    };

    std::size_t version = 0;
    std::vector<Heap> heap;
    std::vector<MemoryStat> total;
    std::vector<MemoryStat> system;
    std::vector<MemoryStat> aspace;
};

NX_REFLECTION_INSTRUMENT(MallocInfo::MemoryStat, (type)(count)(size))
NX_REFLECTION_INSTRUMENT(MallocInfo::Heap::Sizes::Size, (from)(to)(total)(count))
NX_REFLECTION_INSTRUMENT(MallocInfo::Heap::Sizes, (size)(unsorted))
NX_REFLECTION_INSTRUMENT(MallocInfo::Heap, (nr)(sizes)(total)(system)(aspace))
NX_REFLECTION_INSTRUMENT(MallocInfo, (version)(heap)(total)(system)(aspace))

/**
 * @return false on error. Use SystemError::getLastOsErrorCode() to get error description.
 */
NX_UTILS_API bool mallocInfo(
    std::string* data,
    std::string* contentType);

NX_UTILS_API MallocInfo fromXml(const std::string& xml);

} // namespace nx::utils::memory
