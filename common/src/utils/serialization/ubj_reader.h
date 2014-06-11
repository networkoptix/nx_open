#ifndef QN_UBJ_READER_H
#define QN_UBJ_READER_H

#include <cassert>

#include <algorithm> /* For std::min. */

#include <QtCore/QtGlobal>

#include "binary_stream.h"
#include "ubj_fwd.h"
#include "ubj_marker.h"
#include "ubj_detail.h"


template<class Input>
class QnUbjReader: private QnUbjDetail::ReaderWriterBase {
public:
    QnUbjReader(Input *data): 
        m_stream(data),
        m_peeked(false), 
        m_peekedMarker(QnUbj::InvalidMarker) 
    {
        m_stateStack.push_back(State(AtArrayElement));
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

    bool readUInt8(quint8 *target) {
        return readNumberInternal(QnUbj::UInt8Marker, target);
    }

    bool readInt8(qint8 *target) {
        return readNumberInternal(QnUbj::Int8Marker, target);
    }

    bool readInt16(qint16 *target) {
        return readNumberInternal(QnUbj::Int16Marker, target);
    }

    bool readInt32(qint32 *target) {
        return readNumberInternal(QnUbj::Int32Marker, target);
    }

    bool readInt64(qint64 *target) {
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

    bool readBinaryData(QByteArray *target) {
        assert(target);

        if(!readArrayStart())
            return false;

        State &state = m_stateStack.back();
        if(state.type != QnUbj::UInt8Marker)
            return false;

        if(!m_stream.readBytes(state.count, target))
            return false;

        state.status = AtArrayEnd;

        if(!readArrayEnd())
            return false;

        return true;
    }

    bool readArrayStart(int *size = NULL, QnUbj::Marker *type = NULL) {
        return readContainerStartInternal(QnUbj::ArrayStartMarker, AtArrayStart, AtArrayElement, AtSizedArrayElement, AtTypedSizedArrayElement, AtArrayEnd, size, type);
    }

    bool readArrayEnd() {
        return readContainerEndInternal(QnUbj::ArrayEndMarker);
    }

    bool readObjectStart(int *size = NULL, QnUbj::Marker *type = NULL) {
        return readContainerStartInternal(QnUbj::ObjectStartMarker, AtObjectStart, AtObjectKey, AtSizedObjectKey, AtTypedSizedObjectKey, AtObjectEnd, size, type);
    }

    bool readObjectEnd() {
        return readContainerEndInternal(QnUbj::ObjectEndMarker);
    }

private:
    template<class T>
    bool readNumberInternal(QnUbj::Marker expectedMarker, T *target) {
        assert(target);

        peekMarker();
        if(m_peekedMarker != expectedMarker) 
            return false;
        m_peeked = false;

        if(!m_stream.readNumber(target))
            return false;

        return true;
    }

    bool readUtf8StringInternal(QnUbj::Marker expectedMarker, QByteArray *target) {
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

    bool readContainerStartInternal(QnUbj::Marker expectedMarker, Status startStatus, Status normalStatus, Status sizedStatus, Status typedSizedStatus, Status endStatus, int *size, QnUbj::Marker *type) {
        peekMarker();
        if(m_peekedMarker != expectedMarker)
            return false;
        m_peeked = false;

        m_stateStack.push_back(State(startStatus));
        State &state = m_stateStack.back();

        peekMarker();
        if(m_peekedMarker == QnUbj::ContainerTypeMarker) {
            m_peeked = false;

            state.type = m_stream.readMarker();
            if(!QnUbj::isValidContainerType(state.type))
                return false;

            if(m_stream.readMarker() != QnUbj::ContainerSizeMarker)
                return false;

            if(!readSizeFromStream(&state.count))
                return false;

            state.status = state.count == 0 ? endStatus : typedSizedStatus;
        } else if(m_peekedMarker == QnUbj::ContainerSizeMarker) {
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

    bool readContainerEndInternal(QnUbj::Marker expectedMarker) {
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

    QnUbj::Marker readMarkerInternal() {
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
            return QnUbj::ArrayEndMarker;

        case AtObjectStart:
            return m_stream.readNonNoopMarker();

        case AtObjectKey:
            state.status = AtObjectValue;
            return QnUbj::Utf8StringMarker;

        case AtObjectValue:
            state.status = AtObjectKey;
            return m_stream.readNonNoopMarker();

        case AtSizedObjectKey:
            state.status = AtSizedObjectValue;
            return QnUbj::Utf8StringMarker;

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
            return QnUbj::Utf8StringMarker;

        case AtTypedSizedObjectValue:
            state.count--;
            if(state.count == 0) {
                state.status = AtObjectEnd;
            } else {
                state.status = AtSizedObjectKey;
            }
            return state.type;

        case AtObjectEnd:
            return QnUbj::ObjectEndMarker;

        default:
            return QnUbj::InvalidMarker;
        }
    }

    bool readSizeFromStream(int *target) {
        switch (m_stream.readMarker()) {
        case QnUbj::UInt8Marker:    return readTypedSizeFromStream<quint8>(target);
        case QnUbj::Int8Marker:     return readTypedSizeFromStream<qint8>(target);
        case QnUbj::Int16Marker:    return readTypedSizeFromStream<qint16>(target);
        case QnUbj::Int32Marker:    return readTypedSizeFromStream<qint32>(target);
        default:                    return false;
        }
    }

    template<class T>
    bool readTypedSizeFromStream(int *target) {
        T tmp;
        if(!m_stream.readNumber(&tmp))
            return false;

        *target = static_cast<int>(*target);
        if(*target < 0)
            return false;

        return true;
    }

private:
    QnUbjDetail::InputStreamWrapper<Input> m_stream;
    QVector<State> m_stateStack;
    bool m_peeked;
    QnUbj::Marker m_peekedMarker;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnUbjReader<Output> *disable_user_conversions(QnUbjReader<Output> *value) {
    return value;
}


#endif // QN_UBJ_READER_H
