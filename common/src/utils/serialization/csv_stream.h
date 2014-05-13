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
        // unicode symbols will not escaped http://en.wikipedia.org/wiki/UTF-8
        QByteArray utf8 = field.toUtf8();
        writeLatin1Field(utf8);
    }

    //escaping rule: http://www.csvreader.com/csv/docs/DataStreams.Csv.CsvReader.UserSettings.EscapeMode.html
    void writeLatin1Field(const QByteArray &field) 
    {
        bool needEscaping = false;
        unsigned char ch;
        for (int i = 0 ; i < field.size() ; i++)
        {
            ch = field[i];
            if (ch < 32 || ch == ',' || ch == '\\')
                needEscaping = true;
        };
        if (needEscaping)
        {
            QByteArray changedField;
            for (int i = 0 ; i < field.size() ; i++)
            {
                ch = field[i];
                if ( ch < 32 ) {
                    changedField.push_back('\\');
                    switch (ch)
                    {
                    case '\n':  changedField.push_back('n'); break;
                    case '\r':  changedField.push_back('r'); break;
                    case '\t':  changedField.push_back('t'); break;
                    case '\b':  changedField.push_back('b'); break;
                    case '\f':  changedField.push_back('f'); break;
                    case  27 :  changedField.push_back('e'); break;
                    case '\v':  changedField.push_back('v'); break;
                    case '\a':  changedField.push_back('a'); break;
                    default: 
                        {
                            changedField.push_back('x');
                            changedField.push_back(toHexChar(ch / 16));
                            changedField.push_back(toHexChar(ch % 16));
                            break;
                        }
                    }
                } 
                else {
                    if ( ch == ',' || ch == '\\' )
                        changedField.push_back('\\');
                    changedField.push_back(ch);
                }
            };
            m_stream.write(changedField.data(), changedField.size());
        } else m_stream.write(field.data(), field.size());
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
    unsigned char toHexChar(unsigned char digit)
    {
        return digit <= 9 ? '0' + digit : 'A' + (digit-10);
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
