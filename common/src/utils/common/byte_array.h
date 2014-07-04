#ifndef QN_BYTE_ARRAY_H
#define QN_BYTE_ARRAY_H

#include <QtCore/QByteArray>
#include <QtCore/qglobal.h>

#include <utils/memory/abstract_allocator.h>

class QByteArray;

/**
 * Container class for aligned memory chunks.
 */
class QN_EXPORT QnByteArray {
public:
    /**
     * Constructor.
     * 
     * \param alignment                 Alignment of array data.
     * \param capacity                  Initial array capacity.
     */
    explicit QnByteArray( unsigned int alignment, unsigned int capacity );
    explicit QnByteArray( QnAbstractAllocator* const allocator, unsigned int alignment, unsigned int capacity );
    //!Takes external buffer \a data. This buffer is not deleted in destructor!
    explicit QnByteArray( char* buf, unsigned int dataSize );

    /**
     * Destructor.
     */
    ~QnByteArray();

    /**
     * Clears this byte array. Note that capacity remains unchanged.
     */
    void clear();

    /**
     * \returns                         Pointer to the data stored in this array.
     */
    inline const char *constData() const { return m_data + m_ignore; }

    /**
     * \returns                         Pointer to the data stored in this array.
     */
    inline const char *data() const { return constData(); }

    /**
     * \returns                         Pointer to the data stored in this array.
     */
    char *data();

    /**
     * \returns                         Size of this array.
     */
    inline unsigned int size() const { return m_size - m_ignore; }

    /**
     * \returns                         Capacity of this array.
     */
    inline unsigned int capacity() const { return m_capacity; }

    /**
     * \param data                      Pointer to the data to append to this array
     * \param size                      Size of the data to append.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    unsigned int write(const char *data, unsigned int size);

    unsigned int write(quint8 value);

    /**
     * Write to buffer without any checks. Buffer MUST be preallocated before that call
     * \param data                      Pointer to the data to append to this array
     * \param size                      Size of the data to append.
     */
    inline void uncheckedWrite( const char *data, unsigned int size )
    {
        Q_ASSERT_X(m_size + size <= m_capacity, "Buffer MUST be preallocated!", Q_FUNC_INFO);
        memcpy(m_data + m_size, data, size);
        m_size += size;
    }


    /**
     * \param data                      Data to append to this array.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    inline unsigned int write(const QByteArray &data) { return write(data.constData(), data.size()); }

    /**
     * \param data                      Data to append to this array.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    inline unsigned int write(const QnByteArray &data) { return write(data.constData(), data.size()); }

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
    unsigned int writeAt(const char *data, unsigned int size, int pos);

    /**
     * Reserves given amount of bytes in this array and returns a pointer to 
     * the reserved memory region.
     *
     * This function is to be used when some external mechanism is employed 
     * for writing into memory. <tt>finishWriting(unsigned int)</tt> must be
     * called after external writing operation is complete.
     * 
     * \param size                      Number of bytes to reserve for writing.
     * \returns                         Pointer to the reserved memory region.
     */
    char *startWriting(unsigned int size);

    /**
     * \param size                      Number of bytes that were appended to this
     *                                  array using external mechanisms.
     */
    inline void finishWriting(unsigned int size) { m_size += size; }

    /**
     * Removes trailing zero bytes from this array.
     */
    void removeTrailingZeros();

    /**
     * Attempts to allocate memory for at least the given number of bytes.
     * 
     * \param size                      Number of bytes to reserve.
     */
    void reserve(unsigned int size);

    /**
     * \param size                      New size for this array.
     */
    void resize(unsigned int size);




    /* Deprecated functions follow. */

    // TODO: #Elric this function breaks data alignment.
    void ignore_first_bytes(int bytes_to_ignore);

    int getAlignment() const;

    QnByteArray& operator=( const QnByteArray& );
    QnByteArray& operator=( QnByteArray&& );

protected:
    bool reallocate(unsigned int capacity);

private:
    QnAbstractAllocator* m_allocator;
    unsigned int m_alignment;
    unsigned int m_capacity;
    unsigned int m_size;
    char *m_data;
    int m_ignore;
    //!true, if we own memory pointed to by \a m_data
    bool m_ownBuffer;

    QnByteArray( const QnByteArray& );
};

#endif // QN_BYTE_ARRAY_H
