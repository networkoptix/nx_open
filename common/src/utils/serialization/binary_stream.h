#ifndef QN_SERIALIZATION_BINARY_STREAM_H
#define QN_SERIALIZATION_BINARY_STREAM_H

#include "binary_fwd.h"

template<>
class QnInputBinaryStream<QByteArray> {
public:
    QnInputBinaryStream(const QByteArray &data): 
        m_data(data),
        m_pos(0)
    {}

    /*!
        \return Bytes actually read
    */
    int read(void *buffer, int maxSize) {
        int toRead = qMin(m_data.size() - m_pos, maxSize);
        memcpy(buffer, m_data.constData() + m_pos, toRead);
        m_pos += toRead;
        return toRead;
    }

    //!Resets internal cursor position
    void reset() {
        m_pos = 0;
    }

    const QByteArray &buffer() const { return m_data; }
    int pos() const { return m_pos; } // TODO: #Elric is this one needed?

private:
    const QByteArray& m_data;
    int m_pos;
};


template<>
class QnOutputBinaryStream<QByteArray> {
public:
    QnOutputBinaryStream(QByteArray *data): 
        m_data( data )
    {}

    int write(const void *data, int size) {
        m_data->append(static_cast<const char *>(data), size);
        return size;
    }

private:
    QByteArray *m_data;
};


/* Disable ADL wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Input>
QnInputBinaryStream<Input> *adl_wrap(QnInputBinaryStream<Input> *value) {
    return value;
}

template<class Output>
QnOutputBinaryStream<Output> adl_wrap(QnOutputBinaryStream<Output> *value) {
    return value;
}


#endif // QN_SERIALIZATION_BINARY_STREAM_H
