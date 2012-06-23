#ifndef CLBYTEARRAY_H
#define CLBYTEARRAY_H

#include <QByteArray>
#include <QtCore/qglobal.h>

class QByteArray;

// the purpose of this class is to provide array container for alignment data
class QN_EXPORT CLByteArray
{
public:
    // creates array with alignment and capacity
    // note that capacity() function might return slightly different value from the one passed to constructor
    explicit CLByteArray(unsigned int alignment, unsigned int capacity);
    ~CLByteArray();

    // sets current write position and size to 0; capacity remains unchanged
    void clear();

    // returns const pointer to aligned data
    const char *constData() const;
    inline const char *data() const
    { return constData(); }

    // returns pointer to aligned data
    char *data();

    // returns actual data size
    unsigned int size() const;

    unsigned int capacity() const;

    // writes size bytes to array starting from current position.
    // if size > (capacity - curr_size) then capacity automatically increased by factor of 2
    unsigned int write(const char *data, unsigned int size);

    //writes QByteArray to array starting from current position.
    // if size > (capacity - curr_size) then capacity automatically increased by factor of 2
    inline unsigned int write(const QByteArray &data)
    { return write(data.constData(), data.size()); }

    //writes size bytes to array starting from abs_shift position.
    // if size + shift > (capacity) then capacity automatically increased by factor of 2
    unsigned int write(const char *data, unsigned int size, int abs_shift);

    // writes other array to current position
    // if other.size > (this.capacity - this.curr_size) then capacity automatically increased by factor of 2
    inline unsigned int write(const CLByteArray &data)
    { return write(data.constData(), data.size()); }

    // this function makes sure that array has capacity to write size bytes
    // returns a pointer
    // this function is used with done()
    char *prepareToWrite(unsigned int size);

    void done(unsigned int size);

    void resize(unsigned int size);

    void ignore_first_bytes(int bytes_to_ignore);

    //======

    //av cam
    void removeZerosAtTheEnd();

    // fill data
    void fill(quint8 filler, int size);

    // this function reallocates data and changes capacity.
    // if new capacity smaller than current size this function does nothing
    // return false if can not allocate new memory
    bool reallocate(unsigned int new_capacity);
private:
    Q_DISABLE_COPY(CLByteArray)

private:
    unsigned int m_alignment;
    unsigned int m_capacity;
    unsigned int m_size;

    char *m_data;

    int m_ignore;
};

#endif // CLBYTEARRAY_H
