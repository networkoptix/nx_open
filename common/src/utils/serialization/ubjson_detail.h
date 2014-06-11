#ifndef QN_UBJSON_DETAIL_H
#define QN_UBJSON_DETAIL_H

#include <QtCore/QtGlobal>

#include "binary_stream.h"
#include "ubjson_fwd.h"
#include "ubjson_marker.h"

namespace QnUbjsonDetail {

    template<class T>
    T fromBigEndian(const T &value) {
        return qFromBigEndian(value);
    }

    inline char fromBigEndian(char value) {
        return value;
    }

    inline float fromBigEndian(float value) {
        quint32 tmp = reinterpret_cast<quint32 &>(value);
        tmp = qFromBigEndian(tmp);
        return reinterpret_cast<float &>(tmp);
    }

    inline float fromBigEndian(double value) {
        quint64 tmp = reinterpret_cast<quint64 &>(value);
        tmp = qFromBigEndian(tmp);
        return reinterpret_cast<double &>(tmp);
    }

    template<class T>
    T toBigEndian(const T &value) {
        return qToBigEndian(value);
    }

    inline char toBigEndian(char value) {
        return value;
    }

    inline float toBigEndian(float value) {
        quint32 tmp = reinterpret_cast<quint32 &>(value);
        tmp = qToBigEndian(tmp);
        return reinterpret_cast<float &>(tmp);
    }

    inline double toBigEndian(double value) {
        quint64 tmp = reinterpret_cast<quint64 &>(value);
        tmp = qToBigEndian(tmp);
        return reinterpret_cast<double &>(tmp);
    }


    class ReaderWriterBase {
    protected:
        enum Status {
            AtArrayStart,
            AtArrayElement,
            AtSizedArrayElement,
            AtTypedSizedArrayElement,
            AtArrayEnd,

            AtObjectStart,
            AtObjectKey,
            AtObjectValue,
            AtSizedObjectKey,
            AtSizedObjectValue,
            AtTypedSizedObjectKey,
            AtTypedSizedObjectValue,
            AtObjectEnd
        };

        struct State {
            State(): status(AtArrayElement), type(QnUbjson::InvalidMarker), count(-1) {}
            State(Status status): status(status), type(QnUbjson::InvalidMarker), count(-1) {}

            Status status;
            QnUbjson::Marker type;
            int count;
        };
    };


    template<class Input>
    class InputStreamWrapper {
    public:
        InputStreamWrapper(Input *data): m_stream(data) {}

        QnUbjson::Marker readMarker() {
            char c;
            if(m_stream.read(&c, 1) != 1) {
                return QnUbjson::InvalidMarker;
            } else {
                return QnUbjson::markerFromChar(c);
            }
        }

        QnUbjson::Marker readNonNoopMarker() {
            while(true) {
                QnUbjson::Marker result = readMarker();
                if(result != QnUbjson::NoopMarker)
                    return result;
            }
        }

        template<class T>
        bool readNumber(T *target) {
            if(m_stream.read(target, sizeof(T)) != sizeof(T))
                return false;

            *target = fromBigEndian(*target);
            return true;
        }

        bool readBytes(int size, QByteArray *target) {
            const int chunkSize = 16 * 1024 * 1024;
            if(size < chunkSize) {
                /* If it's less than 16Mb then just read it as a whole chunk. */
                target->resize(size);
                return m_stream.read(target->data(), size) == size;
            } else {
                /* Otherwise there is a high probability that the stream is corrupted, 
                 * but we cannot be 100% sure. Read it chunk-by-chunk, then assemble. */
                QVector<QByteArray> chunks;

                for(int pos = 0; pos < size; pos += chunkSize) {
                    QByteArray chunk;
                    chunk.resize(std::min(chunkSize, size - pos));

                    if(m_stream.read(chunk.data(), chunk.size()) != chunk.size())
                        return false; /* The stream was indeed corrupted. */

                    chunks.push_back(chunk);
                }

                /* Assemble the chunks. */
                target->clear();
                target->reserve(size);
                for(const QByteArray chunk: chunks)
                    target->append(chunk);
                return true;
            }
        }

    private:
        QnInputBinaryStream<Input> m_stream;
    };

    
    template<class Output>
    class OutputStreamWrapper {
    public:
        OutputStreamWrapper(Output *data): m_stream(data) {}

        void writeMarker(QnUbjson::Marker marker) {
            char c = QnUbjson::charFromMarker(marker);
            m_stream.write(&c, 1);
        }

        template<class T>
        void writeNumber(T value) {
            value = toBigEndian(value);
            m_stream.write(&value, sizeof(T));
        }

        void writeBytes(const QByteArray &value) {
            m_stream.write(value.data(), value.size());
        }

    private:
        QnOutputBinaryStream<Output> m_stream;
    };


} // namespace QnUbjsonDetail

#endif // QN_UBJSON_DETAIL_H
