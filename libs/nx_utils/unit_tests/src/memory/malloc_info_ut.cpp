// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/memory/malloc_info.h>

#include <nx/reflect/json.h>

namespace nx::utils::memory::test {

static constexpr char kXml[] = R"xml(
<malloc version="1">
<heap nr="1">
<sizes>
  <size from="17" to="32" total="64" count="2"/>
  <size from="33" to="48" total="96" count="2"/>
  <size from="49" to="64" total="64" count="1"/>
  <unsorted from="33" to="21857" total="31370" count="10"/>
</sizes>
<total type="fast" count="6" size="352"/>
<total type="rest" count="49974" size="320695061"/>
<system type="current" size="634847232"/>
<system type="max" size="634847232"/>
<aspace type="total" size="634847232"/>
<aspace type="mprotect" size="634847232"/>
<aspace type="subheaps" size="10"/>
</heap>
<heap nr="2">
<sizes>
  <size from="117" to="132" total="164" count="12"/>
  <unsorted from="144" to="121857" total="131370" count="110"/>
</sizes>
<total type="fast" count="16" size="1352"/>
<total type="rest" count="149974" size="1320695061"/>
<system type="current" size="1634847232"/>
<system type="max" size="1634847232"/>
<aspace type="total" size="1634847232"/>
<aspace type="mprotect" size="1634847232"/>
<aspace type="subheaps" size="110"/>
</heap>
<total type="fast" count="1378" size="123184"/>
<total type="rest" count="654321" size="123456"/>
<total type="mmap" count="10" size="10"/>
<system type="current" size="123456"/>
<system type="max" size="123456"/>
<aspace type="total" size="123456"/>
<aspace type="mprotect" size="123456"/>
</malloc>
)xml";

TEST(MallocInfo, fromXml)
{
    auto mallocInfo = fromXml(kXml);
    ASSERT_EQ(1, mallocInfo.version);

    ASSERT_EQ(2, mallocInfo.heap.size());

    memory::MallocInfo::Heap heap0Expected {
        .nr = 1,
        .sizes = {
            .size = {{
                {17, 32, 64, 2},
                {33, 48, 96, 2},
                {49, 64, 64, 1},
            }},
            .unsorted = memory::MallocInfo::Heap::Sizes::Size{33, 21857, 31370, 10}
        },
        .total = {{
            memory::MallocInfo::MemoryStat{.type = "fast", .count = 6, .size = 352},
            memory::MallocInfo::MemoryStat{.type = "rest", .count = 49974, .size = 320695061}
        }},
        .system = {{
            memory::MallocInfo::MemoryStat{.type = "current", .size = 634847232},
            memory::MallocInfo::MemoryStat{.type = "max", .size = 634847232}
        }},
        .aspace = {{
            memory::MallocInfo::MemoryStat{.type = "total", .size = 634847232},
            memory::MallocInfo::MemoryStat{.type = "mprotect", .size = 634847232},
            memory::MallocInfo::MemoryStat{.type = "subheaps", .size = 10}
        }}
    };

    ASSERT_EQ(heap0Expected.nr, mallocInfo.heap[0].nr);
    ASSERT_EQ(heap0Expected.sizes.size, mallocInfo.heap[0].sizes.size);
    ASSERT_EQ(heap0Expected.sizes.unsorted, mallocInfo.heap[0].sizes.unsorted);
    ASSERT_EQ(heap0Expected.total, mallocInfo.heap[0].total);
    ASSERT_EQ(heap0Expected.system, mallocInfo.heap[0].system);
    ASSERT_EQ(heap0Expected.aspace, mallocInfo.heap[0].aspace);

    std::vector<memory::MallocInfo::MemoryStat> totalExpected {{
        {"fast", 1378, 123184},
        {"rest", 654321, 123456},
        {"mmap", 10, 10}}};
    ASSERT_EQ(totalExpected, mallocInfo.total);

    std::vector<memory::MallocInfo::MemoryStat> systemExepcted {{
        memory::MallocInfo::MemoryStat{.type = "current", .size = 123456},
        memory::MallocInfo::MemoryStat{.type = "max", .size = 123456},
    }};
    ASSERT_EQ(systemExepcted, mallocInfo.system);

    std::vector<memory::MallocInfo::MemoryStat> aspaceExepcted {{
        memory::MallocInfo::MemoryStat{.type = "total", .size = 123456},
        memory::MallocInfo::MemoryStat{.type = "mprotect", .size = 123456},
    }};
    ASSERT_EQ(aspaceExepcted, mallocInfo.aspace);
}

} //namespace nx::utils::memory::test
