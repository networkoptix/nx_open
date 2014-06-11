#ifndef QN_UBJ_WRITER_H
#define QN_UBJ_WRITER_H

#include <cassert>

#include <QtCore/QtGlobal>

#include "binary_stream.h"
#include "ubj_fwd.h"
#include "ubj_writer.h"
#include "ubj_detail.h"

template<class Output>
class QnUbjWriter: private QnUbjDetail::ReaderWriterBase {
public:
    QnUbjWriter(Output *data): 
        m_stream(data)
    {
        m_stateStack.push_back(State(AtArrayElement));
    }

    void writeNull() {
        writeMarkerInternal(QnUbj::NullMarker);
    }

    void writeBool(bool value) {
        writeMarkerInternal(value ? QnUbj::TrueMarker : QnUbj::FalseMarker);
    }

    void writeUInt8(quint8 value) {
        return writeNumberInternal(QnUbj::UInt8Marker, value);
    }

    void writeInt8(qint8 value) {
        return writeNumberInternal(QnUbj::Int8Marker, value);
    }

    void writeInt16(qint16 value) {
        return writeNumberInternal(QnUbj::Int16Marker, value);
    }

    void writeInt32(qint32 value) {
        return writeNumberInternal(QnUbj::Int32Marker, value);
    }

    void writeInt64(qint64 value) {
        return writeNumberInternal(QnUbj::Int64Marker, value);
    }

    void writeFloat(float value) {
        return writeNumberInternal(QnUbj::FloatMarker, value);
    }

    void writeDouble(double value) {
        return writeNumberInternal(QnUbj::DoubleMarker, value);
    }

    void writeBigNumber(const QByteArray &value) {
        return writeUtf8StringInternal(QnUbj::BigNumberMarker, value);
    }

    void writeLatin1Char(char value) {
        return writeNumberInternal(QnUbj::Latin1CharMarker, value);
    }

    void writeUtf8String(const QByteArray &value) {
        return writeUtf8StringInternal(QnUbj::Utf8StringMarker, value);
    }

    void writeUtf8String(const QString &value) {
        writeUtf8String(value.toUtf8());
    }

    void writeBinaryData(const QByteArray &value) {
        writeArrayStart(value.size(), QnUbj::UInt8Marker);

        m_stream.writeBytes(value);

        State &state = m_stateStack.back();
        state.status = AtArrayEnd;

        writeArrayEnd();
    }

    void writeArrayStart(int size = -1, QnUbj::Marker type = QnUbj::InvalidMarker) {
        writeContainerStartInternal(QnUbj::ArrayStartMarker, AtArrayStart, AtArrayElement, AtSizedArrayElement, AtTypedSizedArrayElement, AtArrayEnd, size, type);
    }

    void writeArrayEnd() {
        writeContainerEndInternal(QnUbj::ArrayEndMarker);
    }

    void writeObjectStart(int size = -1, QnUbj::Marker type = QnUbj::InvalidMarker) {
        writeContainerStartInternal(QnUbj::ObjectStartMarker, AtObjectStart, AtObjectKey, AtSizedObjectKey, AtTypedSizedObjectKey, AtObjectEnd, size, type);
    }

    void writeObjectEnd() {
        writeContainerEndInternal(QnUbj::ObjectEndMarker);
    }

private:
    template<class T>
    void writeNumberInternal(QnUbj::Marker marker, T value) {
        writeMarkerInternal(marker);
        m_stream.writeNumber(value);
    }

    void writeUtf8StringInternal(QnUbj::Marker marker, const QByteArray &value) {
        writeMarkerInternal(marker);
        writeSizeToStream(value.size());
        m_stream.writeBytes(value);
    }

    void writeContainerStartInternal(QnUbj::Marker marker, Status startStatus, Status normalStatus, Status sizedStatus, Status typedSizedStatus, Status endStatus, int size, QnUbj::Marker type) {
        writeMarkerInternal(marker);

        m_stateStack.push_back(State(startStatus));
        State &state = m_stateStack.back();

        if(type != QnUbj::InvalidMarker) {
            assert(QnUbj::isValidContainerType(type) && size >= 0);

            m_stream.writeMarker(QnUbj::ContainerTypeMarker);
            m_stream.writeMarker(type);
            m_stream.writeMarker(QnUbj::ContainerSizeMarker);
            writeSizeToStream(size);

            state.status = state.count == 0 ? endStatus : typedSizedStatus;
        } else if(size >= 0) {
            m_stream.writeMarker(QnUbj::ContainerSizeMarker);
            writeSizeToStream(size);

            state.status = state.count == 0 ? endStatus : sizedStatus;
        } else {
            state.status = normalStatus;
        }
    }

    void writeContainerEndInternal(QnUbj::Marker marker) {
        assert(m_stateStack.size() > 1);

        State &state = m_stateStack.back();
        assert(state.count <= 0);

        writeMarkerInternal(marker);

        m_stateStack.pop_back();
    }

    void writeMarkerInternal(QnUbj::Marker marker) {
        State &state = m_stateStack.back();
        switch (state.status) {
        case AtArrayStart:
            m_stream.writeMarker(marker);
            break;

        case AtArrayElement:
            m_stream.writeMarker(marker);
            break;

        case AtSizedArrayElement:
            state.count--;
            if(state.count == 0)
                state.status = AtArrayEnd;
            m_stream.writeMarker(marker);
            break;

        case AtTypedSizedArrayElement:
            assert(marker == state.type);
            state.count--;
            if(state.count == 0)
                state.status = AtArrayEnd;
            break;

        case AtArrayEnd:
            assert(marker == QnUbj::ArrayEndMarker);
            break;

        case AtObjectStart:
            m_stream.writeMarker(marker);
            break;

        case AtObjectKey:
            assert(marker == QnUbj::Utf8StringMarker);
            state.status = AtObjectValue;
            break;

        case AtObjectValue:
            state.status = AtObjectKey;
            m_stream.writeMarker(marker);
            break;

        case AtSizedObjectKey:
            assert(marker == QnUbj::Utf8StringMarker);
            state.status = AtSizedObjectValue;
            break;

        case AtSizedObjectValue:
            state.count--;
            if(state.count == 0) {
                state.status = AtObjectEnd;
            } else {
                state.status = AtSizedObjectKey;
            }
            m_stream.writeMarker(marker);
            break;

        case AtTypedSizedObjectKey:
            assert(marker == QnUbj::Utf8StringMarker);
            state.status = AtTypedSizedObjectValue;
            break;

        case AtTypedSizedObjectValue:
            assert(marker == state.type);
            state.count--;
            if(state.count == 0) {
                state.status = AtObjectEnd;
            } else {
                state.status = AtSizedObjectKey;
            }
            break;

        case AtObjectEnd:
            assert(marker == QnUbj::ObjectEndMarker);
            break;

        default:
            break;
        }
    }

    void writeSizeToStream(int value) {
        if(value <= 255) {
            m_stream.writeMarker(QnUbj::UInt8Marker);
            m_stream.writeNumber(static_cast<quint8>(value));
        } else if (value <= 32767) {
            m_stream.writeMarker(QnUbj::Int16Marker);
            m_stream.writeNumber(static_cast<qint16>(value));
        } else {
            m_stream.writeMarker(QnUbj::Int32Marker);
            m_stream.writeNumber(static_cast<qint32>(value));
        }
    }

private:
    QnUbjDetail::OutputStreamWrapper<Output> m_stream;
    QVector<State> m_stateStack;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnUbjWriter<Output> *disable_user_conversions(QnUbjWriter<Output> *value) {
    return value;
}


#endif // QN_UBJ_WRITER_H
