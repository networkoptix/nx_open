#ifndef QN_SERIALIZATION_CSV_STREAM_H
#define QN_SERIALIZATION_CSV_STREAM_H

#include "binary_stream.h"

template<class Output>
class QnCsvStreamWriter {
public:
    QnCsvStreamWriter(Output *data):
        m_stream(data)
    {
    }

    void writeField(const QString &field) 
    {
        QByteArray utf8 = field.toUtf8();
        writeLatin1Field(utf8);
    }

    unsigned char toHexChar(unsigned char digit)
    {
        return digit <= 9 ? '0' + digit : 'A' + (digit-10);
    }

    void writeLatin1Field(const QByteArray &field) 
    {
        QByteArray changed_field;
        for (int i = 0 ; i < field.size() ; i++)
        {
            unsigned char ch = field[i];
            if ( ch < 32 ) {
                changed_field.push_back('\\');
                changed_field.push_back(toHexChar(ch / 16));
                changed_field.push_back(toHexChar(ch % 16));
            } 
            else {
                if ( ch == ',' || ch == '\\' )
                    changed_field.push_back('\\');
                changed_field.push_back(ch);
            }
        };
        m_stream.write(changed_field.data(), changed_field.size());
    }

    void writeLatin1Field(const char *field) {
        writeLatin1Field(QByteArray::fromRawData(field, qstrlen(field)));
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
