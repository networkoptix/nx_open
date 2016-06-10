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

    void writeField(const QString &field) {
        writeUtf8Field(field.toUtf8());
    }

    /**
     * Writes out the given utf-8 encoded field. Escaping is done as described here:
     * http://www.csvreader.com/csv/docs/DataStreams.Csv.CsvReader.UserSettings.EscapeMode.html
     * 
     * \param field
     */ 
    void writeUtf8Field(const QByteArray &field) {
        bool needEscaping = false;
        unsigned char ch;
        for (int i = 0 ; i < field.size() ; i++) {
            ch = field[i];
            if (ch < 32 || ch == ',' || ch == '\\') {
                needEscaping = true;
                break;
            }
        }

        if (!needEscaping) {
            m_stream.write(field.data(), field.size());
            return;
        }

        QByteArray changedField;
        for (int i = 0 ; i < field.size() ; i++) {
            ch = field[i];

            /* Note that we won't end up escaping partial UTF-8 sequences here
             * as all of them are in range [128, 255]. See http://en.wikipedia.org/wiki/UTF-8. */

            if ( ch < 32 ) {
                changedField.push_back('\\');
                switch (ch) {
                case '\n':  changedField.push_back('n'); break;
                case '\r':  changedField.push_back('r'); break;
                case '\t':  changedField.push_back('t'); break;
                default:
                    changedField.push_back('x');
                    changedField.push_back(toHexChar(ch / 16));
                    changedField.push_back(toHexChar(ch % 16));
                    break;
                }
            } else {
                if ( ch == ',' || ch == '\\' )
                    changedField.push_back('\\');
                changedField.push_back(ch);
            }
        }
        m_stream.write(changedField.data(), changedField.size());
    }

    void writeUtf8Field(const char *field) {
        writeUtf8Field(QByteArray::fromRawData(field, qstrlen(field)));
    }

    void writeComma() {
        m_stream.write(",", 1);
    }

    void writeEndline() {
        m_stream.write("\r\n", 2); /* RFC 4180: CRLF line endings in CSV. */
    }

private:
    static unsigned char toHexChar(unsigned char digit) {
        return digit <= 9 ? '0' + digit : 'A' + (digit-10);
    }

private:
    QnOutputBinaryStream<Output> m_stream;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnCsvStreamWriter<Output> *disable_user_conversions(QnCsvStreamWriter<Output> *value) {
    return value;
}


#endif // QN_SERIALIZATION_CSV_STREAM_H
