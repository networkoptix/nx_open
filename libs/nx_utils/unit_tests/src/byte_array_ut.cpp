// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <nx/utils/byte_array.h>

namespace nx::utils::test {

namespace {

constexpr size_t kPadding = 64;

void assertPaddingIsZeroed(const ByteArray& array)
{
    ASSERT_EQ(kPadding, array.paddingSize());
    const char* dataEnd = array.constData() + array.size();
    for (size_t i = 0; i < kPadding; ++i)
        ASSERT_EQ(0, dataEnd[i]) << "Padding byte " << i;
}

} // namespace

TEST(ByteArray, paddingIsZeroedAfterWrite)
{
    ByteArray array(/*alignment*/ 16, /*capacity*/ 0, kPadding);

    // Every write grows the capacity with a reserve, so the padding does not coincide with the
    // zeroed tail of the allocated buffer.
    for (int i = 0; i < 10; ++i)
    {
        array.write("0123456789", 10);
        assertPaddingIsZeroed(array);
    }

    array.write((quint8) 42);
    assertPaddingIsZeroed(array);

    array.writeFiller(0xff, 10);
    assertPaddingIsZeroed(array);

    array.writeAt("abc", 3, 0); //< Does not change the size.
    assertPaddingIsZeroed(array);
}

TEST(ByteArray, paddingIsZeroedAfterDataIsReplaced)
{
    ByteArray array(/*alignment*/ 16, /*capacity*/ 1024, kPadding);
    array.write(std::string(1000, 'x').c_str(), 1000);

    // The stale data must not be visible in the padding after the array is reused or shrunk.
    array.clear();
    assertPaddingIsZeroed(array);

    array.write("abc", 3);
    assertPaddingIsZeroed(array);

    array.resize(500);
    assertPaddingIsZeroed(array);

    array.resize(2);
    assertPaddingIsZeroed(array);

    char* buffer = array.startWriting(100);
    memset(buffer, 'y', 100);
    array.finishWriting(100);
    assertPaddingIsZeroed(array);
}

TEST(ByteArray, paddingIsZeroedAfterCopy)
{
    ByteArray array(/*alignment*/ 16, /*capacity*/ 1024, kPadding);
    array.write(std::string(1000, 'x').c_str(), 1000);

    ByteArray copy(array);
    assertPaddingIsZeroed(copy);

    ByteArray moved(std::move(copy));
    assertPaddingIsZeroed(moved);
}

} // namespace nx::utils::test
