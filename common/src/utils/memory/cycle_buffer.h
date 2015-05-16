#ifndef __QN_CYCLE_BUFFER_H_
#define __QN_CYCLE_BUFFER_H_

/*!
    Fixed size cyclic buffer implementation.
*/

class QnMediaCycleBuffer
{
public:
    typedef int size_type;
    typedef char value_type;
    
    /*!
        Constructor
        \param bufferSize bufferSize
        \param align memory align factor for internal buffer
    */
    QnMediaCycleBuffer(size_type bufferSize, int align);
    ~QnMediaCycleBuffer();

    //!Insert data to the middle of buffer. Extend buffer size if required
    void insert(size_type pos, const value_type* data, size_type size);

    //!Add data to the end of the buffer
    void push_back(const value_type* data, size_type size);

    //!Remove data from front
    void pop_front(size_type size);

    /*!
        Return external data buffer. It may cause memory copy operation to defragment internal buffer.
        In most cases it doesn't require any memory copy operations. QByteArray is construct as reference to the internal buffer.
    */
    const QByteArray data(size_type pos = 0, size_type size = -1);

    //!Return maximum data size, that can be captured via 'data' call without memory copying.
    size_type seriesDataSize(size_type pos = 0) const;

    //!Return buffer size
    size_type size() const { return m_size; }

    //!Clear data
    void clear() { m_size = m_offset = 0; }

    //!Return allocated buffer size. Cyclic buffer can't be extended, so size() MUST be <= maxSize()
    size_type maxSize() const { return m_maxSize; }
private:
    void reallocateBuffer();
private:
    value_type* m_buffer;
    size_type m_maxSize;
    size_type m_size;
    size_type m_offset;
};

#endif // _QN_CYCLE_BUFFER_H_
