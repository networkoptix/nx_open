#ifndef QN_UBJSON_READER_H
#define QN_UBJSON_READER_H

#include <cassert>

#include <algorithm> /* For std::min. */

#include <QtCore/QtGlobal>
#include <QtCore/QVarLengthArray>

#include "binary_stream.h"
#include "ubjson_fwd.h"
#include "ubjson_marker.h"
#include "ubjson_detail.h"


template<class Input>
class QnUbjsonReader: private QnUbjsonDetail::ReaderWriterBase {
public:
    QnUbjsonReader(const Input *data): 
        m_stream(data),
        m_peeked(false), 
        m_peekedMarker(QnUbjson::InvalidMarker) 
    {
        m_stateStack.push_back(State(AtArrayElement));
    }

    QnUbjson::Marker peekMarker() {
        if(m_peeked)
            return m_peekedMarker;

        m_peekedMarker = readMarkerInternal();
        m_peeked = true;

        return m_peekedMarker;
    }

    bool readNull() {
        peekMarker();
        if(m_peekedMarker != QnUbjson::NullMarker) 
            return false;
        
        m_peeked = false;
        return true;
    }

    bool readBool(bool *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker == QnUbjson::TrueMarker) {
            m_peeked = false;
            *target = true;
            return true;
        } else if(m_peekedMarker == QnUbjson::FalseMarker) {
            m_peeked = false;
            *target = false;
            return true;
        } else {
            return false;
        }
    }

    bool readUInt8(quint8 *target) {
        return readNumberInternal(QnUbjson::UInt8Marker, target);
    }

    bool readInt8(qint8 *target) {
        return readNumberInternal(QnUbjson::Int8Marker, target);
    }

    bool readInt16(qint16 *target) {
        return readNumberInternal(QnUbjson::Int16Marker, target);
    }

    bool readInt32(qint32 *target) {
        return readNumberInternal(QnUbjson::Int32Marker, target);
    }

    bool readInt64(qint64 *target) {
        return readNumberInternal(QnUbjson::Int64Marker, target);
    }

    bool readUInt64(quint64 *target) {
        return readNumberInternal(QnUbjson::Int64Marker, (qint64*)target);
    }

    bool readFloat(float *target) {
        // TODO: #Elric support NaN (like in JSON spec)
        return readNumberInternal(QnUbjson::FloatMarker, target);
    }

    bool readDouble(double *target) {
        // TODO: #Elric support NaN (like in JSON spec)
        return readNumberInternal(QnUbjson::DoubleMarker, target);
    }

    bool readBigNumber(QByteArray *target) {
        return readUtf8StringInternal(QnUbjson::BigNumberMarker, target);
    }

    bool readLatin1Char(char *target) {
        return readNumberInternal(QnUbjson::Latin1CharMarker, target);
    }

    bool readUtf8String(QByteArray *target) {
        return readUtf8StringInternal(QnUbjson::Utf8StringMarker, target);
    }

    bool readUtf8String(QString *target) {
        assert(target);

        QByteArray tmp;
        if(!readUtf8String(&tmp))
            return false;

        *target = QString::fromUtf8(tmp);
        return true;
    }

    bool readBinaryData(QByteArray *target) {
        assert(target);

        if(!readArrayStart())
            return false;

        State &state = m_stateStack.back();
        if(state.type != QnUbjson::UInt8Marker)
            return false;

        if(!m_stream.readBytes(state.count, target))
            return false;

        state.status = AtArrayEnd;

        if(!readArrayEnd())
            return false;

        return true;
    }

    bool readArrayStart(int *size = NULL, QnUbjson::Marker *type = NULL) {
        return readContainerStartInternal<AtArrayStart, AtArrayElement, AtSizedArrayElement, AtTypedSizedArrayElement, AtArrayEnd>(QnUbjson::ArrayStartMarker, size, type);
    }

    bool readArrayEnd() {
        return readContainerEndInternal(QnUbjson::ArrayEndMarker);
    }

    bool readObjectStart(int *size = NULL, QnUbjson::Marker *type = NULL) {
        return readContainerStartInternal<AtObjectStart, AtObjectKey, AtSizedObjectKey, AtTypedSizedObjectKey, AtObjectEnd>(QnUbjson::ObjectStartMarker, size, type);
    }

    bool readObjectEnd() {
        return readContainerEndInternal(QnUbjson::ObjectEndMarker);
    }

    bool skipValue() {
        QnUbjson::Marker marker = peekMarker();
        m_peeked = false;

        switch (marker) {
        case QnUbjson::NullMarker:
        case QnUbjson::TrueMarker: 
        case QnUbjson::FalseMarker:
            return true;
        case QnUbjson::UInt8Marker:
            return m_stream.skipBytes(sizeof(quint8));
        case QnUbjson::Int8Marker:
            return m_stream.skipBytes(sizeof(qint8));
        case QnUbjson::Int16Marker:
            return m_stream.skipBytes(sizeof(qint16));
        case QnUbjson::Int32Marker:
            return m_stream.skipBytes(sizeof(qint32));
        case QnUbjson::Int64Marker:
            return m_stream.skipBytes(sizeof(qint64));
        case QnUbjson::FloatMarker:
            return m_stream.skipBytes(sizeof(float));
        case QnUbjson::DoubleMarker:
            return m_stream.skipBytes(sizeof(double));
        case QnUbjson::Latin1CharMarker:
            return m_stream.skipBytes(sizeof(char));
        case QnUbjson::BigNumberMarker:
        case QnUbjson::Utf8StringMarker: {
            int size;
            if(!readSizeFromStream(&size))
                return false;
            return m_stream.skipBytes(size);
        }
        case QnUbjson::ArrayStartMarker: {
            if(!readArrayStart())
                return false;

            while(true) {
                marker = peekMarker();
                if(marker == QnUbjson::ArrayEndMarker)
                    break;

                skipValue();
            }

            if(!readArrayEnd())
                return false;

            return true;
        }
        case QnUbjson::ObjectStartMarker: {
            if(!readObjectStart())
                return false;

            while(true) {
                marker = peekMarker();
                if(marker == QnUbjson::ObjectEndMarker)
                    break;

                skipValue();
                skipValue();
            }

            if(!readObjectEnd())
                return false;

            return true;
        }
        default:
            return false;
        }
    }
    int pos() const { return m_stream.pos(); }
private:
    template<class T>
    bool readNumberInternal(QnUbjson::Marker expectedMarker, T *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker != expectedMarker) 
            return false;
        m_peeked = false;

        if(!m_stream.readNumber(target))
            return false;

        return true;
    }

    bool readUtf8StringInternal(QnUbjson::Marker expectedMarker, QByteArray *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        int size;
        if(!readSizeFromStream(&size))
            return false;

        return m_stream.readBytes(size, target);
    }

    template<Status startStatus, Status normalStatus, Status sizedStatus, Status typedSizedStatus, Status endStatus>
    bool readContainerStartInternal(QnUbjson::Marker expectedMarker, int *size, QnUbjson::Marker *type) {
        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        m_stateStack.push_back(State(startStatus));
        State &state = m_stateStack.back();

        peekMarker();
        if(m_peekedMarker == QnUbjson::ContainerTypeMarker) {
            m_peeked = false;

            state.type = m_stream.readMarker();
            if(!QnUbjson::isValidContainerType(state.type))
                return false;

            if(m_stream.readMarker() != QnUbjson::ContainerSizeMarker)
                return false;

            if(!readSizeFromStream(&state.count))
                return false;

            state.status = state.count == 0 ? endStatus : typedSizedStatus;
        } else if(m_peekedMarker == QnUbjson::ContainerSizeMarker) {
            m_peeked = false;

            state.status = sizedStatus;

            if(!readSizeFromStream(&state.count))
                return false;

            state.status = state.count == 0 ? endStatus : sizedStatus;
        } else {
            state.status = normalStatus;
        }

        if(size)
            *size = state.count;
        if(type)
            *type = state.type;

        return true;
    }

    bool readContainerEndInternal(QnUbjson::Marker expectedMarker) {
        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        m_stateStack.pop_back();
        if(m_stateStack.isEmpty()) {
            /* Upper level array should not be closed. */
            m_stateStack.push_back(State(AtArrayElement));
            return false;
        }

        return true;
    }

    QnUbjson::Marker readMarkerInternal() {
        assert(!m_peeked);

        State &state = m_stateStack.back();
        switch (state.status) {
        case AtArrayStart:
            return m_stream.readNonNoopMarker();

        case AtArrayElement:
            return m_stream.readNonNoopMarker();

        case AtSizedArrayElement:
            state.count--;
            if(state.count == 0)
                state.status = AtArrayEnd;
            return m_stream.readNonNoopMarker();

        case AtTypedSizedArrayElement:
            state.count--;
            if(state.count == 0)
                state.status = AtArrayEnd;
            return state.type;

        case AtArrayEnd:
            return QnUbjson::ArrayEndMarker;

        case AtObjectStart:
            return m_stream.readNonNoopMarker();

        case AtObjectKey:
            state.status = AtObjectValue;
            return QnUbjson::Utf8StringMarker;

        case AtObjectValue:
            state.status = AtObjectKey;
            return m_stream.readNonNoopMarker();

        case AtSizedObjectKey:
            state.status = AtSizedObjectValue;
            return QnUbjson::Utf8StringMarker;

        case AtSizedObjectValue:
            state.count--;
            if(state.count == 0) {
                state.status = AtObjectEnd;
            } else {
                state.status = AtSizedObjectKey;
            }
            return m_stream.readNonNoopMarker();

        case AtTypedSizedObjectKey:
            state.status = AtTypedSizedObjectValue;
            return QnUbjson::Utf8StringMarker;

        case AtTypedSizedObjectValue:
            state.count--;
            if(state.count == 0) {
                state.status = AtObjectEnd;
            } else {
                state.status = AtSizedObjectKey;
            }
            return state.type;

        case AtObjectEnd:
            return QnUbjson::ObjectEndMarker;

        default:
            return QnUbjson::InvalidMarker;
        }
    }

    bool readSizeFromStream(int *target) {
        // TODO: #Elric #ubjson support Int64 here

        switch (m_stream.readMarker()) {
        case QnUbjson::UInt8Marker: return readTypedSizeFromStream<quint8>(target);
        case QnUbjson::Int8Marker:  return readTypedSizeFromStream<qint8>(target);
        case QnUbjson::Int16Marker: return readTypedSizeFromStream<qint16>(target);
        case QnUbjson::Int32Marker: return readTypedSizeFromStream<qint32>(target);
        default:                    return false;
        }
    }

    template<class T>
    bool readTypedSizeFromStream(int *target) {
        T tmp;
        if(!m_stream.readNumber(&tmp))
            return false;

        *target = static_cast<int>(tmp);
        if(*target < 0)
            return false;

        return true;
    }

private:
    QnUbjsonDetail::InputStreamWrapper<Input> m_stream;
    QVarLengthArray<State, 8> m_stateStack;
    bool m_peeked;
    QnUbjson::Marker m_peekedMarker;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnUbjsonReader<Output> *disable_user_conversions(QnUbjsonReader<Output> *value) {
    return value;
}


#endif // QN_UBJSON_READER_H
