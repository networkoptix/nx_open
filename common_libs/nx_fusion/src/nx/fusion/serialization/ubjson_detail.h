#ifndef QN_UBJSON_DETAIL_H
#define QN_UBJSON_DETAIL_H

#include <QtCore/QtGlobal>
#include <QtCore/QtEndian>

#include "binary_stream.h"
#include "ubjson_fwd.h"
#include "ubjson_marker.h"

namespace QnUbjsonDetail {

    template<class T>
    T fromBigEndian(const char* data)
    {
        T value = *((T*) data);
        return qFromBigEndian(value);
    }

    template<>
    inline char fromBigEndian(const char* data)
    {
        return data[0];
    }

    template<>
    inline float fromBigEndian(const char* data)
    {
        union {
            quint32 i;
            float f;
        } tmp;

        tmp.i = qFromBigEndian(*((quint32*) data));
        return tmp.f;
    }

    template<>
    inline double fromBigEndian(const char* data)
    {
        union {
            quint64 i;
            double d;
        } tmp;

        tmp.i = qFromBigEndian(*((quint64*) data));
        return tmp.d;
    }

    template<class T>
    T toBigEndian(const T &value) {
        return qToBigEndian(value);
    }

    inline char toBigEndian(char value) {
        return value;
    }

    inline quint32 toBigEndian(float value) {
        /* Avoid breaking strict aliasing rules by using a union. */
        union {
            quint32 i;
            float f;
        } tmp;

        tmp.f = value;
        return qToBigEndian(tmp.i);
    }

    inline quint64 toBigEndian(double value) {
        /* Avoid breaking strict aliasing rules by using a union. */
        union {
            quint64 i;
            double f;
        } tmp;

        tmp.f = value;
        return qToBigEndian(tmp.i);
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
        InputStreamWrapper(const Input *data): m_stream(data) {}

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
        bool readNumber(T *target)
        {
            char data[sizeof(T)];
            if(m_stream.read(data, sizeof(T)) != sizeof(T))
                return false;

            *target = fromBigEndian<T>(data);
            return true;
        }

        bool readBytes(int size, char *target) {
            return m_stream.read(target, size) == size;
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

        bool skipBytes(int size) {
            return m_stream.skip(size) == size;
        }

        int pos() const {
            return m_stream.pos();
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
            auto bigEndianValue = toBigEndian(value);
            static_assert(sizeof(T) == sizeof(bigEndianValue), "Invalid type conversion");
            m_stream.write(&bigEndianValue, sizeof(bigEndianValue));
        }

        void writeBytes(const QByteArray &value) {
            m_stream.write(value.data(), value.size());
        }

    private:
        QnOutputBinaryStream<Output> m_stream;
    };


} // namespace QnUbjsonDetail

#endif // QN_UBJSON_DETAIL_H
