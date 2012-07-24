#ifndef QN_BYTE_ARRAY_H
#define QN_BYTE_ARRAY_H

#include <QByteArray>
#include <QtCore/qglobal.h>

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
    explicit QnByteArray(unsigned int alignment, unsigned int capacity);

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
    const char *constData() const;

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
    unsigned int size() const;

    /**
     * \returns                         Capacity of this array.
     */
    unsigned int capacity() const;

    /**
     * \param data                      Pointer to the data to append to this array
     * \param size                      Size of the data to append.
     * \returns                         New size of this array, or 0 in case of an error.
     */
    unsigned int write(const char *data, unsigned int size);

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
    void write(quint8 filler, int size);

    /**
     * Overwrites the contents of this array starting at the given position
     * with the supplied data.
     * 
     * \param data                      Data to use for overwriting.
     * \param size                      Size of the data.
     * \param pos                       Position to overwrite.
     */
    unsigned int write(const char *data, unsigned int size, int pos);

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
    void finishWriting(unsigned int size);

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

    // TODO: this function breaks data alignment.
    void ignore_first_bytes(int bytes_to_ignore);

protected:
    bool reallocate(unsigned int capacity);

private:
    Q_DISABLE_COPY(QnByteArray)

private:
    unsigned int m_alignment;
    unsigned int m_capacity;
    unsigned int m_size;
    char *m_data;
    int m_ignore;
};

#endif // QN_BYTE_ARRAY_H
