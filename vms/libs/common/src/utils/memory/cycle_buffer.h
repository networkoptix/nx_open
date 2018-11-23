#pragma once

#include <vector>

/*!
    Fixed size cyclic buffer implementation.
*/

class QnMediaCyclicBuffer
{
public:
    typedef int size_type;
    typedef char value_type;

    struct Range
    {
        Range(const value_type* data, size_type size): data(data), size(size) {}
        
        const value_type* data;
        size_type size;
    };

    /*!
        Constructor
        \param bufferSize bufferSize
        \param align memory align factor for internal buffer
    */
    QnMediaCyclicBuffer(size_type bufferSize, int align);
    ~QnMediaCyclicBuffer();

    //!Insert data to the middle of buffer. Extend buffer size if required
    void insert(size_type pos, const value_type* data, size_type size);

    //!Add data to the end of the buffer
    void push_back(const value_type* data, size_type size);

    //!Remove data from front
    void pop_front(size_type size);

    /*!
        Return pointer to data buffer. It may cause memory copy operation to defragment internal buffer.
        This pointer is guarantee that requested buffer is accessible as a single serial data block at least for size bytes
        \param pos data offset.
        \param size data size. -1 means all data.
    */
    const value_type* unfragmentedData(size_type pos = 0, size_type size = -1);

    //!Return buffer list covered requested range of the data
    std::vector<Range> fragmentedData(size_type pos = 0, size_type size = -1) const;

    //!Return buffer size
    size_type size() const { return m_size; }

    bool resize(size_type size);

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
    int m_align;
};
