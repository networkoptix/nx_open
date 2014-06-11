#ifndef QN_UBJ_READER_H
#define QN_UBJ_READER_H

#include <cassert>
#include <cstdint>

#include <algorithm> /* For std::min. */

#include "binary_stream.h"
#include "ubj_marker.h"


template<class T>
class QnUbjReader {
public:
    QnUbjReader(QnInputBinaryStream<T> *stream): 
        m_stream(stream),
        m_peeked(false), 
        m_peekedMarker(QnUbj::InvalidMarker) 
    {
        m_parseStack.push_back(ParserState(AtArrayElement));
    }

    QnUbj::Marker peekMarker() {
        if(m_peeked)
            return m_peekedMarker;

        m_peekedMarker = readMarkerInternal();
        m_peeked = true;

        return m_peekedMarker;
    }

    bool readNull() {
        peekMarker();
        if(m_peekedMarker != QnUbj::NullMarker) 
            return false;
        
        m_peeked = false;
        return true;
    }

    bool readBool(bool *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker == QnUbj::TrueMarker) {
            m_peeked = false;
            *target = true;
            return true;
        } else if(m_peekedMarker == QnUbj::FalseMarker) {
            m_peeked = false;
            *target = false;
            return true;
        } else {
            return false;
        }
    }

    bool readUInt8(std::uint8_t *target) {
        return readNumberInternal(QnUbj::UInt8Marker, target);
    }

    bool readInt8(std::int8_t *target) {
        return readNumberInternal(QnUbj::Int8Marker, target);
    }

    bool readInt16(std::int16_t *target) {
        return readNumberInternal(QnUbj::Int16Marker, target);
    }

    bool readInt32(std::int32_t *target) {
        return readNumberInternal(QnUbj::Int32Marker, target);
    }

    bool readInt64(std::int64_t *target) {
        return readNumberInternal(QnUbj::Int64Marker, target);
    }

    bool readFloat(float *target) {
        return readNumberInternal(QnUbj::FloatMarker, target);
    }

    bool readDouble(double *target) {
        return readNumberInternal(QnUbj::DoubleMarker, target);
    }

    bool readBigNumber(QByteArray *target) {
        return readUtf8StringInternal(QnUbj::BigNumberMarker, target);
    }

    bool readLatin1Char(char *target) {
        return readNumberInternal(QnUbj::Latin1CharMarker, target);
    }

    bool readUtf8String(QByteArray *target) {
        return readUtf8StringInternal(QnUbj::Utf8StringMarker, target);
    }

    bool readUtf8String(QString *target) {
        assert(target);

        QByteArray tmp;
        if(!readUtf8String(&tmp))
            return false;

        *target = QString::fromUtf8(tmp);
        return true;
    }

    bool readArrayStart(int *size = NULL, QnUbj::Marker *type = NULL) {
        return readContainerStartInternal(QnUbj::ArrayStartMarker, AtArrayElement, AtSizedArrayElement, AtTypedSizedArrayElement, AtArrayEnd, size, type);
    }

    bool readUInt8ArrayData(QByteArray *target) {
        assert(target);

        ParserState &parserState = m_parseStack.back();

        if(parserState.state != AtTypedSizedArrayElement || parserState.type != QnUbj::UInt8Marker)
            return false;

        if(!readBytesInternal(parserState.count, target))
            return false;

        parserState.state = AtArrayEnd;
        return true;
    }

    bool readArrayEnd() {
        return readContainerEndInternal(QnUbj::ArrayEndMarker);
    }

    bool readObjectStart(int *size, QnUbj::Marker *type) {
        return readContainerStartInternal(QnUbj::ObjectStartMarker, AtObjectKey, AtSizedObjectKey, AtTypedSizedObjectKey, AtObjectEnd, size, type);
    }

    bool readObjectEnd() {
        return readContainerEndInternal(QnUbj::ObjectEndMarker);
    }

private:
    template<class Number>
    bool readNumberInternal(QnUbj::Marker expectedMarker, Number *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker != expectedMarker) 
            return false;
        m_peeked = false;

        if(m_stream->read(target, sizeof(Number)) != sizeof(Number))
            return false;

        *target = fromBigEndian(*target);
        return true;
    }

    bool readUtf8StringInternal(QnUbj::Marker expectedMarker, QByteArray *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        int size;
        if(!readSizeInternal(&size))
            return false;

        return readBytesInternal(size, target);
    }

    bool readBytesInternal(int size, QByteArray *target) {
        const int chunkSize = 16 * 1024 * 1024;
        if(size < chunkSize) {
            /* If it's less than 16Mb then just read it as a whole chunk. */
            target->resize(size);
            return m_stream->read(target->data(), size) == size;
        } else {
            /* Otherwise there is a high probability that the stream is corrupted, 
             * but we cannot be 100% sure. Read it chunk-by-chunk, then assemble. */
            QVector<QByteArray> chunks;

            for(int pos = 0; pos < size; pos += chunkSize) {
                QByteArray chunk;
                chunk.resize(std::min(chunkSize, size - pos));

                if(m_stream->read(chunk.data(), chunk.size()) != chunk.size())
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

    bool readSizeInternal(int *target) {
        switch (peekMarker()) {
        case QnUbj::UInt8Marker:    return readSizeInternal<std::uint8_t>(QnUbj::UInt8Marker, target);
        case QnUbj::Int8Marker:     return readSizeInternal<std::int8_t>(QnUbj::Int8Marker, target);
        case QnUbj::Int16Marker:    return readSizeInternal<std::int16_t>(QnUbj::Int16Marker, target);
        case QnUbj::Int32Marker:    return readSizeInternal<std::int32_t>(QnUbj::Int32Marker, target);
        default:                    return false;
        }
    }

    template<class Number>
    bool readSizeInternal(QnUbj::Marker expectedMarker, int *target) {
        Number tmp;
        if(!readNumberInternal(expectedMarker, &tmp))
            return false;

        *target = static_cast<int>(target);
        if(*target < 0)
            return false;

        return true;
    }

    bool readContainerStartInternal(QnUbj::Marker expectedMarker, State normalState, State sizedState, State typedSizedState, State endState, int *size, QnUbj::Marker *type) {
        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        ParserState parserState;

        peekMarker();
        if(m_peekedMarker == QnUbj::ContainerTypeMarker) {
            m_peeked = false;

            parserState.state = typedSizedState;

            parserState.type = readMarkerFromStream();
            if(!QnUbj::isValidContainerType(parserState.type))
                return false;

            if(readMarkerFromStream() != QnUbj::ContainerSizeMarker)
                return false;

            if(!readSizeInternal(&parserState.count))
                return false;
            if(parserState.count == 0)
                parserState.state = endState;
        } else if(m_peekedMarker == QnUbj::ContainerSizeMarker) {
            m_peeked = false;

            parserState.state = sizedState;

            if(!readSizeInternal(&state.count))
                return false;
            if(parserState.count == 0)
                parserState.state = endState;
        } else {
            parserState.state = normalState;
        }

        if(size)
            *size = parserState.count;
        if(type)
            *type = parserState.type;

        m_parseStack.push_back(parserState);
        return true;
    }

    bool readContainerEndInternal(QnUbj::Marker expectedMarker) {
        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        m_parseStack.pop_back();
        if(m_parseStack.isEmpty()) {
            /* Upper level array should not be closed. */
            m_parseStack.push_back(ParserState(AtArrayElement));
            return false;
        }

        return true;
    }

    QnUbj::Marker readMarkerInternal() {
        assert(!m_peeked);

        const ParserState &parserState = m_parseStack.back();
        switch (parserState.state) {
        case AtArrayElement:
            return readNonNoopMarkerFromStream();

        case AtSizedArrayElement:
            parserState.count--;
            if(parserState.count == 0)
                parserState.state = AtArrayEnd;
            return readNonNoopMarkerFromStream();

        case AtTypedSizedArrayElement:
            parserState.count--;
            if(parserState.count == 0)
                parserState.state = AtArrayEnd;
            return parserState.type;

        case AtArrayEnd:
            return QnUbj::ArrayEndMarker;

        case AtObjectKey:
            parserState.state = AtObjectValue;
            return QnUbj::Utf8StringMarker;

        case AtObjectValue:
            parserState.state = AtObjectKey;
            return readNonNoopMarkerFromStream();

        case AtSizedObjectKey:
            parserState.state = AtSizedObjectValue;
            return QnUbj::Utf8StringMarker;

        case AtSizedObjectValue:
            parserState.count--;
            if(parserState.count == 0) {
                parserState.state = AtObjectEnd;
            } else {
                parserState.state = AtSizedObjectKey;
            }
            return readNonNoopMarkerFromStream();

        case AtTypedSizedObjectKey:
            parserState.state = AtTypedSizedObjectValue;
            return QnUbj::Utf8StringMarker;

        case AtTypedSizedObjectValue:
            parserState.count--;
            if(parserState.count == 0) {
                parserState.state = AtObjectEnd;
            } else {
                parserState.state = AtSizedObjectKey;
            }
            return parserState.type;

        case AtObjectEnd:
            return QnUbj::ObjectEndMarker;

        default:
            break;
        }
    }

    QnUbj::Marker readNonNoopMarkerFromStream() {
        while(true) {
            QnUbj::Marker result = readMarkerFromStream();
            if(result != QnUbj::NoopMarker)
                return result;
        }
    }

    QnUbj::Marker readMarkerFromStream() {
        char c;
        if(m_stream->read(&c, 1) != 1) {
            return QnUbj::InvalidMarker;
        } else {
            return QnUbj::markerFromChar(c);
        }
    }

    template<class Number>
    static Number fromBigEndian(const Number &value) {
        return qFromBigEndian(value);
    }

    static char fromBigEndian(char value) {
        return value;
    }

    static float fromBigEndian(float value) {
        std::uint32_t tmp = reinterpret_cast<std::uint32_t &>(value);
        tmp = qFromBigEndian(tmp);
        return reinterpret_cast<float &>(tmp);
    }

    static float fromBigEndian(double value) {
        std::uint64_t tmp = reinterpret_cast<std::uint64_t &>(value);
        tmp = qFromBigEndian(tmp);
        return reinterpret_cast<double &>(tmp);
    }

private:
    enum State {
        AtArrayElement,
        AtSizedArrayElement,
        AtTypedSizedArrayElement,
        AtArrayEnd,
        
        AtObjectKey,
        AtObjectValue,
        AtSizedObjectKey,
        AtSizedObjectValue,
        AtTypedSizedObjectKey,
        AtTypedSizedObjectValue,
        AtObjectEnd
    };

    struct ParserState {
        ParserState(): state(QnUbj::AtArrayElement), type(QnUbj::InvalidMarker), count(-1) {}
        ParserState(State state): state(state), type(QnUbj::InvalidMarker), count(-1) {}

        State state;
        QnUbj::Marker type;
        int count;
    };

    QnInputBinaryStream<T> *m_stream;
    bool m_peeked;
    QnUbj::Marker m_peekedMarker;
    QVector<ParserState> m_parseStack;
};


#endif // QN_UBJ_READER_H
