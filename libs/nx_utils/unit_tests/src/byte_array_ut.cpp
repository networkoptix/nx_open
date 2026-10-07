// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include <nx/utils/byte_array.h>

namespace nx::utils::test {

namespace {

constexpr size_t kAlignment = 32;
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
    ByteArray array(/*capacity*/ 0, /*alignment*/ 16, kPadding);

    // Every write grows the capacity with a reserve, so the padding does not coincide with the
    // zeroed tail of the allocated buffer.
    for (int i = 0; i < 10; ++i)
    {
        array.write("0123456789", 10);
        assertPaddingIsZeroed(array);
    }

    array.write("*", 1);
    assertPaddingIsZeroed(array);

    array.writeFiller(0xff, 10);
    assertPaddingIsZeroed(array);

    array.writeAt("abc", 3, 0); //< Does not change the size.
    assertPaddingIsZeroed(array);
}

TEST(ByteArray, paddingIsZeroedAfterDataIsReplaced)
{
    ByteArray array(/*capacity*/ 1024, /*alignment*/ 16, kPadding);
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
    ByteArray array(/*capacity*/ 1024, /*alignment*/ 16, kPadding);
    array.write(std::string(1000, 'x').c_str(), 1000);

    ByteArray copy(array);
    assertPaddingIsZeroed(copy);

    ByteArray moved(std::move(copy));
    assertPaddingIsZeroed(moved);
}

TEST(ByteArray, writeGrowsTheBuffer)
{
    ByteArray array(/*capacity*/ 4, kAlignment, /*padding*/ 0);

    array.write("Hello, ", 7);
    array.write("world", 5);

    ASSERT_EQ(12u, array.size());
    ASSERT_GE(array.capacity(), 12u);
    ASSERT_EQ(0, memcmp(array.constData(), "Hello, world", 12));
}

TEST(ByteArray, reserveThrowsOnUnallocatableSize)
{
    ByteArray array(/*capacity*/ 0, kAlignment, /*padding*/ 0);

    // Without the overflow guard in mallocAligned() this used to allocate a tiny buffer, and the
    // following write() would memcpy into it.
    ASSERT_THROW(array.reserve(std::numeric_limits<size_t>::max()), std::bad_alloc);
}

TEST(ByteArray, reserveThrowsOnCapacityPlusPaddingOverflow)
{
    ByteArray array(/*capacity*/ 0, kAlignment, /*padding*/ 64);

    ASSERT_THROW(array.reserve(std::numeric_limits<size_t>::max()), std::length_error);
}

TEST(ByteArray, theEntryPointsRejectAnOverflowingSize)
{
    constexpr size_t kMax = std::numeric_limits<size_t>::max();

    // The sums below wrap, so reserve() would see a small number, keep the buffer it already has
    // and let the caller write far beyond it. The size has to be rejected where it is added up.
    ByteArray array(/*capacity*/ 128, kAlignment, kPadding);
    array.writeFiller('x', 100);

    ASSERT_THROW(array.write("payload", kMax - 50), std::length_error);
    ASSERT_THROW(array.startWriting(kMax - 50), std::length_error);
    ASSERT_THROW(array.writeAt("payload", kMax - 50, 100), std::length_error);

    // A negative count reaches the same arithmetic as a huge unsigned one.
    ASSERT_THROW(array.writeFiller('x', -1), std::length_error);
    ASSERT_THROW(array.writeAt("payload", 8, -1), std::length_error);

    // The array is untouched by the rejected calls.
    ASSERT_EQ(100u, array.size());
    ASSERT_EQ(128u, array.capacity());
}

TEST(ByteArray, copyAssignmentIsIntactAfterAFailedAllocation)
{
    ByteArray destination(/*capacity*/ 16, kAlignment, /*padding*/ 0);
    destination.write("Hello, world", 12);

    // A padding this large cannot be allocated, so copying from this array fails.
    const ByteArray source(/*capacity*/ 0, kAlignment, std::numeric_limits<size_t>::max() - 8);

    ASSERT_THROW(destination = source, std::bad_alloc);

    // The failed assignment must not have released the buffer of the destination: reading it here
    // and freeing it in the destructor used to be a use-after-free and a double free.
    ASSERT_EQ(12u, destination.size());
    ASSERT_EQ(0, memcmp(destination.constData(), "Hello, world", 12));
}

TEST(ByteArray, copyAssignmentCopiesTheData)
{
    ByteArray source(/*capacity*/ 16, kAlignment, /*padding*/ 0);
    source.write("Hello, world", 12);

    ByteArray destination(/*capacity*/ 4, kAlignment, /*padding*/ 0);
    destination.write("stale", 5);

    destination = source;

    ASSERT_EQ(12u, destination.size());
    ASSERT_EQ(0, memcmp(destination.constData(), "Hello, world", 12));

    // The copy must be independent of the source.
    source.clear();
    ASSERT_EQ(12u, destination.size());
    ASSERT_EQ(0, memcmp(destination.constData(), "Hello, world", 12));
}

TEST(ByteArray, emptyArraysAreCopiedWithoutNullPointers)
{
    // memcpy() and memset() require valid pointers even for a zero length, while an array which
    // was never written to holds a null one. UBSan reports the violation, so these have to stay
    // guarded by a length check.
    const ByteArray empty;
    ASSERT_EQ(nullptr, empty.constData());

    ByteArray destination;
    destination = empty;
    ASSERT_EQ(0u, destination.size());

    const ByteArray copy(empty);
    ASSERT_EQ(0u, copy.size());

    ByteArray array(/*capacity*/ 16, kAlignment, kPadding);
    array.write(empty);
    array.write(nullptr, 0);
    array.writeAt(nullptr, 0, 0);
    array.writeFiller(0, 0);
    ASSERT_EQ(0u, array.size());
}

TEST(ByteArray, moveTakesOverTheWholeBuffer)
{
    ByteArray source(/*capacity*/ 100000, kAlignment, kPadding);
    source.write("Hello, world", 12);
    const char* const buffer = source.constData();

    ByteArray destination;
    destination = std::move(source);

    // The buffer is taken over as it is, so the capacity of the allocation comes along with it.
    // Reporting the size instead used to make the next reserve() reallocate for nothing.
    ASSERT_EQ(buffer, destination.constData());
    ASSERT_EQ(100000u, destination.capacity());
    ASSERT_EQ(12u, destination.size());
    ASSERT_EQ(0, memcmp(destination.constData(), "Hello, world", 12));

    destination.reserve(50000);
    ASSERT_EQ(buffer, destination.constData()) << "The buffer was large enough already";

    // The source must not keep describing a buffer it does not own any more.
    ASSERT_EQ(0u, source.size());
    ASSERT_EQ(0u, source.capacity());
}

TEST(ByteArray, selfCopyAssignmentKeepsTheData)
{
    ByteArray array(/*capacity*/ 16, kAlignment, /*padding*/ 0);
    array.write("Hello, world", 12);

    const ByteArray& alias = array;
    array = alias;

    ASSERT_EQ(12u, array.size());
    ASSERT_EQ(0, memcmp(array.constData(), "Hello, world", 12));
}

} // namespace nx::utils::test
