#ifndef QN_SERIALIZATION_BINARY_STREAM_H
#define QN_SERIALIZATION_BINARY_STREAM_H

#include "binary_fwd.h"

#ifndef QN_NO_QT
#   include <QtCore/QByteArray>
#endif

#ifndef QN_NO_QT

template<>
class QnInputBinaryStream<QByteArray> {
public:
    QnInputBinaryStream(const QByteArray *data): 
        m_data(data),
        m_pos(0)
    {}

    /**
     * \param buffer
     * \param size
     * \returns                         Number of bytes actually read.
     */
    int read(void *buffer, int size) {
        int toRead = qMin(m_data->size() - m_pos, size);
        memcpy(buffer, m_data->constData() + m_pos, toRead);
        m_pos += toRead;
        return toRead;
    }

    int skip(int size) {
        size = qMin(size, m_data->size() - m_pos);
        m_pos += size;
        return size;
    }

    const QByteArray &buffer() const { return *m_data; }
    int pos() const { return m_pos; }

private:
    const QByteArray *m_data;
    int m_pos;
};


template<>
class QnOutputBinaryStream<QByteArray> {
public:
    QnOutputBinaryStream(QByteArray *data): 
        m_data(data)
    {}

    int write(const void *data, int size) {
        m_data->append(static_cast<const char *>(data), size);
        return size;
    }

private:
    QByteArray *m_data;
};

#endif // QN_NO_QT


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Input>
QnInputBinaryStream<Input> *disable_user_conversions(QnInputBinaryStream<Input> *value) {
    return value;
}

template<class Output>
QnOutputBinaryStream<Output> disable_user_conversions(QnOutputBinaryStream<Output> *value) {
    return value;
}


#endif // QN_SERIALIZATION_BINARY_STREAM_H
