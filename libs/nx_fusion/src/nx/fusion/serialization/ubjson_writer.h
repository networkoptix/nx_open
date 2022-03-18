// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_UBJSON_WRITER_H
#define QN_UBJSON_WRITER_H

#include <string>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QtGlobal>
#include <QtCore/QVarLengthArray>

#include "binary_stream.h"
#include "ubjson_fwd.h"
#include "ubjson_detail.h"

template<class Output>
class QnUbjsonWriter: private QnUbjsonDetail::ReaderWriterBase
{
public:
    QnUbjsonWriter(Output *data):
        m_stream(data)
    {
        m_stateStack.push_back(State(AtArrayElement));
    }

    void writeNull()
    {
        writeMarkerInternal(QnUbjson::NullMarker);
    }

    void writeBool(bool value)
    {
        writeMarkerInternal(value ? QnUbjson::TrueMarker : QnUbjson::FalseMarker);
    }

    void writeUInt8(quint8 value)
    {
        return writeNumberInternal(QnUbjson::UInt8Marker, value);
    }

    void writeInt8(qint8 value)
    {
        return writeNumberInternal(QnUbjson::Int8Marker, value);
    }

    void writeChar(char value)
    {
        return writeNumberInternal(QnUbjson::Int8Marker, value);
    }

    void writeUInt16(quint16 value)
    {
        return writeNumberInternal(QnUbjson::Int16Marker, (qint16)value);
    }

    void writeInt16(qint16 value)
    {
        return writeNumberInternal(QnUbjson::Int16Marker, value);
    }

    void writeInt32(qint32 value)
    {
        return writeNumberInternal(QnUbjson::Int32Marker, value);
    }

    void writeInt64(qint64 value)
    {
        return writeNumberInternal(QnUbjson::Int64Marker, value);
    }

    void writeUInt64(quint64 value)
    {
        return writeNumberInternal(QnUbjson::Int64Marker, (qint64)value);
    }

    void writeFloat(float value)
    {
        return writeNumberInternal(QnUbjson::FloatMarker, value);
    }

    void writeDouble(double value)
    {
        return writeNumberInternal(QnUbjson::DoubleMarker, value);
    }

    void writeBigNumber(const QByteArray &value)
    {
        return writeUtf8StringInternal(QnUbjson::BigNumberMarker, value);
    }

    void writeLatin1Char(char value)
    {
        return writeNumberInternal(QnUbjson::Latin1CharMarker, value);
    }

    void writeUtf8String(const QByteArray &value)
    {
        return writeUtf8StringInternal(QnUbjson::Utf8StringMarker, value);
    }

    void writeUtf8String(const QString &value)
    {
        writeUtf8String(value.toUtf8());
    }

    void writeUtf8String(const std::string& value)
    {
        writeUtf8String(QByteArray::fromRawData(value.c_str(), (int) value.size()));
    }

    void writeBinaryData(const QByteArray &value)
    {
        writeArrayStart(value.size(), QnUbjson::UInt8Marker);

        m_stream.writeBytes(value);

        State &state = m_stateStack.back();
        state.count = 0;
        state.status = AtArrayEnd;

        writeArrayEnd();
    }

    void writeJsonValue(const QJsonValue& value)
    {
        switch (value.type())
        {
        case QJsonValue::Null:
            writeNull();
            break;
        case QJsonValue::Bool:
            writeBool(value.toBool());
            break;
        case QJsonValue::Double:
            writeDouble(value.toDouble());
            break;
        case QJsonValue::String:
            writeUtf8String(value.toString());
            break;
        case QJsonValue::Array:
            writeJsonArray(value.toArray());
            break;
        case QJsonValue::Object:
            writeJsonObject(value.toObject());
            break;
        default:
            NX_ASSERT(false, "Unsupported QJsonValue type: %1", value.type());
            writeNull();
            break;
        }
    }

    void writeArrayStart(int size = -1, QnUbjson::Marker type = QnUbjson::InvalidMarker)
    {
        writeContainerStartInternal<AtArrayStart, AtArrayElement, AtSizedArrayElement, AtTypedSizedArrayElement, AtArrayEnd>(QnUbjson::ArrayStartMarker, size, type);
    }

    void writeArrayEnd()
    {
        writeContainerEndInternal(QnUbjson::ArrayEndMarker);
    }

    void writeObjectStart(int size = -1, QnUbjson::Marker type = QnUbjson::InvalidMarker)
    {
        writeContainerStartInternal<AtObjectStart, AtObjectKey, AtSizedObjectKey, AtTypedSizedObjectKey, AtObjectEnd>(QnUbjson::ObjectStartMarker, size, type);
    }

    void writeObjectEnd()
    {
        writeContainerEndInternal(QnUbjson::ObjectEndMarker);
    }

private:
    template<class T>
    void writeNumberInternal(QnUbjson::Marker marker, T value)
    {
        writeMarkerInternal(marker);
        m_stream.writeNumber(value);
    }

    void writeUtf8StringInternal(QnUbjson::Marker marker, const QByteArray &value)
    {
        writeMarkerInternal(marker);
        writeSizeToStream(value.size());
        m_stream.writeBytes(value);
    }

    template<Status startStatus, Status normalStatus, Status sizedStatus, Status typedSizedStatus, Status endStatus>
    void writeContainerStartInternal(QnUbjson::Marker marker, int size, QnUbjson::Marker type)
    {
        writeMarkerInternal(marker);

        m_stateStack.push_back(State(startStatus));
        State &state = m_stateStack.back();

        if(type != QnUbjson::InvalidMarker)
        {
            NX_ASSERT(QnUbjson::isValidContainerType(type) && size >= 0);

            m_stream.writeMarker(QnUbjson::ContainerTypeMarker);
            m_stream.writeMarker(type);
            m_stream.writeMarker(QnUbjson::ContainerSizeMarker);
            writeSizeToStream(size);

            state.type = type;
            state.count = size;
            state.status = state.count == 0 ? endStatus : typedSizedStatus;
        }
        else if(size >= 0)
        {
            m_stream.writeMarker(QnUbjson::ContainerSizeMarker);
            writeSizeToStream(size);

            state.count = size;
            state.status = state.count == 0 ? endStatus : sizedStatus;
        }
        else
        {
            state.status = normalStatus;
        }
    }

    void writeContainerEndInternal(QnUbjson::Marker marker)
    {
        NX_ASSERT(m_stateStack.size() > 1);
        NX_ASSERT(m_stateStack.back().count <= 0);

        writeMarkerInternal(marker);

        m_stateStack.pop_back();
    }

    void writeMarkerInternal(QnUbjson::Marker marker)
    {
        State &state = m_stateStack.back();
        switch (state.status)
        {
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
            NX_ASSERT(marker == state.type);
            state.count--;
            if(state.count == 0)
                state.status = AtArrayEnd;
            break;

        case AtArrayEnd:
            NX_ASSERT(marker == QnUbjson::ArrayEndMarker);
            break;

        case AtObjectStart:
            m_stream.writeMarker(marker);
            break;

        case AtObjectKey:
            NX_ASSERT(marker == QnUbjson::Utf8StringMarker);
            state.status = AtObjectValue;
            break;

        case AtObjectValue:
            state.status = AtObjectKey;
            m_stream.writeMarker(marker);
            break;

        case AtSizedObjectKey:
            NX_ASSERT(marker == QnUbjson::Utf8StringMarker);
            state.status = AtSizedObjectValue;
            break;

        case AtSizedObjectValue:
            state.count--;
            if(state.count == 0)
                state.status = AtObjectEnd;
            else
                state.status = AtSizedObjectKey;
            m_stream.writeMarker(marker);
            break;

        case AtTypedSizedObjectKey:
            NX_ASSERT(marker == QnUbjson::Utf8StringMarker);
            state.status = AtTypedSizedObjectValue;
            break;

        case AtTypedSizedObjectValue:
            NX_ASSERT(marker == state.type);
            state.count--;
            if(state.count == 0)
                state.status = AtObjectEnd;
            else
                state.status = AtSizedObjectKey;
            break;

        case AtObjectEnd:
            NX_ASSERT(marker == QnUbjson::ObjectEndMarker);
            break;

        default:
            break;
        }
    }

    void writeSizeToStream(int value)
    {
        if(value <= 255)
        {
            m_stream.writeMarker(QnUbjson::UInt8Marker);
            m_stream.writeNumber(static_cast<quint8>(value));
        }
        else if (value <= 32767)
        {
            m_stream.writeMarker(QnUbjson::Int16Marker);
            m_stream.writeNumber(static_cast<qint16>(value));
        }
        else
        {
            m_stream.writeMarker(QnUbjson::Int32Marker);
            m_stream.writeNumber(static_cast<qint32>(value));
        }
    }

    void writeJsonArray(const QJsonArray& jsonArray)
    {
        writeArrayStart(jsonArray.size());
        for (const auto& elem: jsonArray)
            writeJsonValue(elem);
        writeArrayEnd();
    }

    void writeJsonObject(const QJsonObject& jsonObject)
    {
        writeObjectStart(jsonObject.size());

        for (auto it = jsonObject.begin(); it != jsonObject.end(); ++it)
        {
            writeUtf8String(it.key());
            writeJsonValue(it.value());
        }

        writeObjectEnd();
    }

private:
    QnUbjsonDetail::OutputStreamWrapper<Output> m_stream;
    QVarLengthArray<State, 8> m_stateStack;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnUbjsonWriter<Output> *disable_user_conversions(QnUbjsonWriter<Output> *value)
{
    return value;
}


#endif // QN_UBJSON_WRITER_H
