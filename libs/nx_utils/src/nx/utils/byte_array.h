// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <cstdint>

namespace nx::utils {

/**
 * Container class for aligned memory chunks.
 *
 * Sizes passed to this class often come from stream parsers, i.e. are externally controlled.
 * Therefore every operation that has to grow the buffer reports a failure to do so by throwing
 * (std::bad_alloc, or std::length_error when the requested size overflows the address space)
 * rather than by silently keeping the old, too small buffer.
 */
class NX_UTILS_API ByteArray
{
public:
    static constexpr size_t kMinAlignment = 16; //< Mirrors nx::kit::utils::kMinAlignment.

    /**
     * @param capacity Initial array capacity.
     * @param alignment Alignment of the array data. Values less than kMinAlignment are silently
     *     raised to it.
     * @param padding Number of extra bytes allocated beyond the array capacity. The padding
     *     which follows the array data is always kept filled with zeros. Used to prevent overread
     *     and segfault for damaged MPEG bitstreams.
     */
    explicit ByteArray(size_t capacity, size_t alignment = kMinAlignment, size_t padding = 0);
    ~ByteArray();

    ByteArray() = default;
    ByteArray(ByteArray&& other) noexcept;
    ByteArray(const ByteArray& other);
    ByteArray& operator=(const ByteArray& right);
    ByteArray& operator=(ByteArray&& source) noexcept;

    /**
     * Clears this byte array. Note that capacity remains unchanged.
     */
    void clear();

    /**
     * Pointer to the data stored in this array.
     */
    const char* constData() const { return m_data; }

    /**
     * Pointer to the data stored in this array.
     */
    const char *data() const { return constData(); }

    /**
     * Pointer to the data stored in this array.
     */
    char *data();

    /**
     * Size of this array.
     */
    size_t size() const { return m_size; }

    /**
     * Capacity of this array.
     */
    size_t capacity() const { return m_capacity; }

    /**
     * Number of bytes which are allocated after the array data and are guaranteed to be filled
     * with zeros, as requested in the constructor.
     */
    size_t paddingSize() const { return m_padding; }

    /**
     * \param data                      Pointer to the data to append to this array
     * \param size                      Size of the data to append.
     * \returns                         Number of bytes written.
     * \throws std::bad_alloc, std::length_error
     */
    size_t write(const char* data, size_t size);

    /**
     * \param data                      Data to append to this array.
     * \returns                         Number of bytes written.
     * \throws std::bad_alloc, std::length_error
     */
    size_t write(const ByteArray& data) { return write(data.constData(), data.size()); }

    /**
     * Appends an array of given size filled with the given byte value to this array.
     *
     * \param filler                    Byte to use for filling the array that will be appended.
     * \param size                      Size of the array that will be appended.
     */
    void writeFiller(uint8_t filler, int size);

    /**
     * Overwrites the contents of this array starting at the given position
     * with the supplied data.
     *
     * \param data                      Data to use for overwriting.
     * \param size                      Size of the data.
     * \param pos                       Position to overwrite.
     */
    size_t writeAt(const char* data, size_t size, int pos);

    /**
     * Reserves given amount of bytes in this array and returns a pointer to
     * the reserved memory region.
     *
     * This function is to be used when some external mechanism is employed
     * for writing into memory. <tt>finishWriting(size_t)</tt> must be
     * called after external writing operation is complete.
     *
     * \param size                      Number of bytes to reserve for writing.
     * \returns                         Pointer to the reserved memory region.
     */
    char* startWriting(size_t size);

    /**
     * \param size                      Number of bytes that were appended to this
     *                                  array using external mechanisms. Must not exceed the
     *                                  amount reserved by startWriting(); a larger value is
     *                                  clamped to the capacity.
     */
    void finishWriting(size_t size);

    /**
     * Allocates memory for at least the given number of bytes.
     *
     * \param size                      Number of bytes to reserve.
     * \throws std::bad_alloc, std::length_error
     */
    void reserve(size_t size);

    /**
     * \param size                      New size for this array.
     */
    void resize(size_t size);

private:
    void reallocate(size_t capacity);

    /**
     * Allocates a buffer of the given capacity plus padding. The padding is left uninitialized -
     * zeroPadding() fills the part of it which follows the data.
     *
     * Takes the alignment and the padding as parameters rather than reading the members, so that
     * it can be called before the members of the target object are modified.
     *
     * \throws std::length_error       If capacity + padding does not fit size_t.
     * \throws std::bad_alloc          If the allocation fails.
     */
    static char* allocateBuffer(size_t capacity, size_t alignment, size_t padding);

    /** Fills the padding which follows the array data with zeros. */
    void zeroPadding();

private:
    size_t m_alignment = kMinAlignment;
    size_t m_capacity = 0;
    size_t m_size = 0;
    size_t m_padding = 0;
    char* m_data = nullptr;
};

} // namespace nx::utils
