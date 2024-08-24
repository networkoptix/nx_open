// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/qglobal.h>

namespace nx::utils {

/**
 * Container class for aligned memory chunks.
 */
class NX_UTILS_API ByteArray
{
public:
    /**
     * @param alignment Alignment of the array data.
     * @param capacity Initial array capacity.
     * @param padding Additional data beyond the array capacity, which will be filled with zeros.
     *     Used to prevent overread and segfault for damaged MPEG bitstreams.
     */
    explicit ByteArray(
        size_t alignment,
        size_t capacity,
        size_t padding);
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
    const char *constData() const { return m_data + m_ignore; }

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
    size_t size() const { return m_size - m_ignore; }

    /**
     * Capacity of this array.
     */
    size_t capacity() const { return m_capacity; }

    /**
     * \param data                      Pointer to the data to append to this array
     * \param size                      Size of the data to append.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    size_t write(const char* data, size_t size);

    size_t write(quint8 value);

    /**
     * Write to buffer without any checks. Buffer MUST be preallocated before that call
     * \param data                      Pointer to the data to append to this array
     * \param size                      Size of the data to append.
     */
    void uncheckedWrite(const char* data, size_t size);

    /**
     * \param data                      Data to append to this array.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    size_t write(const QByteArray& data) { return write(data.constData(), data.size()); }

    /**
     * \param data                      Data to append to this array.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    size_t write(const ByteArray& data) { return write(data.constData(), data.size()); }

    /**
     * Appends an array of given size filled with the given byte value to this array.
     *
     * \param filler                    Byte to use for filling the array that will be appended.
     * \param size                      Size of the array that will be appended.
     */
    void writeFiller(quint8 filler, int size);

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
     *                                  array using external mechanisms.
     */
    void finishWriting(size_t size) { m_size += size; }

    /**
     * Removes trailing zero bytes from this array.
     */
    void removeTrailingZeros(int maxBytesToRemove);

    /**
     * Attempts to allocate memory for at least the given number of bytes.
     *
     * \param size                      Number of bytes to reserve.
     */
    void reserve(size_t size);

    /**
     * \param size                      New size for this array.
     */
    void resize(size_t size);

    /* Deprecated functions follow. */

    // TODO: #sivanov This function breaks data alignment.
    void ignore_first_bytes(size_t bytes_to_ignore);

    int getAlignment() const;

private:
    bool reallocate(size_t capacity);
    char* allocateBuffer(size_t capacity);

private:
    size_t m_alignment = 1;
    size_t m_capacity = 0;
    size_t m_size = 0;
    size_t m_padding = 0;
    size_t m_ignore = 0;
    char* m_data = nullptr;
};

} // namespace nx::utils
