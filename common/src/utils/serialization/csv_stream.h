#ifndef QN_SERIALIZATION_CSV_STREAM_H
#define QN_SERIALIZATION_CSV_STREAM_H

#include "binary_stream.h"

template<class Output>
class QnCsvStreamWriter {
public:
    QnCsvStreamWriter(Output *data):
        m_stream(data)
    {}

    void writeField(const QString &field) {
        QByteArray utf8 = field.toUtf8();
        m_stream.write(utf8.data(), utf8.size()); // TODO: #Elric escaping!
    }

    void writeLatin1Field(const QByteArray &field) {
        m_stream.write(field.data(), field.size()); // TODO: #Elric escaping!
    }

    void writeComma() {
        m_stream.write(",", 1);
    }

    void writeEndline() {
        m_stream.write("\r\n", 2); /* RFC 4180: CRLF line endings in CSV. */
    }

private:
    QnOutputBinaryStream<Output> m_stream;
};


/* Disable ADL wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnCsvStreamWriter<Output> *adl_wrap(QnCsvStreamWriter<Output> *value) {
    return value;
}


#endif // QN_SERIALIZATION_CSV_STREAM_H
