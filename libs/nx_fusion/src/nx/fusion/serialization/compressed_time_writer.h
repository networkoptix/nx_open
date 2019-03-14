#ifndef QN_COMPRESSED_TIME_WRITER_H
#define QN_COMPRESSED_TIME_WRITER_H

#include <cassert>

#include <QtCore/QtGlobal>
#include <QtCore/QVarLengthArray>
#include "QtCore/qendian.h"

#include "binary_stream.h"
#include "compressed_time_fwd.h"
#include "recording/time_period.h"

template<class Output>
class QnCompressedTimeWriter {
public:
    QnCompressedTimeWriter(Output *data, bool signedFormat):
        m_stream(data),
        m_signed(signedFormat),
        m_lastValue(0)
    {
        if (m_signed)
            m_stream.write(QnCompressedTime::SIGNED_FORMAT, sizeof(QnCompressedTime::SIGNED_FORMAT));
        else
            m_stream.write(QnCompressedTime::UNSIGNED_FORMAT, sizeof(QnCompressedTime::UNSIGNED_FORMAT));
    }

    void resetLastValue() {
        m_lastValue = 0;
    }

    /*
    void writeInt64(qint64 value)
    {
        m_signed ? encodeSignedNumber(value - m_lastValue) : encodeUnsignedNumber(value - m_lastValue);
        m_lastValue = value;
    }
    */

    void writeQnTimePeriod(const QnTimePeriod& value)
    {
        m_signed ? encodeSignedNumber(value.startTimeMs - m_lastValue) : encodeUnsignedNumber(value.startTimeMs - m_lastValue);
        encodeUnsignedNumber(value.durationMs + 1); // avoid -1 value
        m_lastValue = value.startTimeMs + value.durationMs;
    }

    void writeQnUuid(const QnUuid& value)
    {
        const QByteArray tmp(value.toRfc4122());
        m_stream.write(tmp.data(), tmp.size());
    }

    void writeSizeToStream(int value)
    {
        writeNumber(value);
    }

    template <class T>
    void writeNumber(const T& value)
    {
        const T& tmp = qToBigEndian(value);
        m_stream.write(&tmp, sizeof(tmp));
    }

private:
    // field header:
    // 00 - 14 bit to data
    // 01 - 22 bit to data
    // 10 - 30 bit to data
    // 11 - 38 bit to data
    // 11 and 0xffffffffff data - 48 bit data

    void encodeUnsignedNumber(qint64 value)
    {
        NX_ASSERT(value >= 0 && value < 0x1000000000000ll);
        if (value < 0x4000ll)
            saveField(value, 0, 2);
        else if (value < 0x400000ll)
            saveField(value, 1, 3);
        else if (value < 0x40000000ll)
            saveField(value, 2, 4);
        else if (value < 0x4000000000ll-1)
            saveField(value, 3, 5);
        else {
            saveField(0xffffffffffll, 3, 5);
            qint64 data = qToBigEndian(value << 16);
            m_stream.write((&data), 6);
        }
    }

    void encodeSignedNumber(qint64 value)
    {
        NX_ASSERT(value >= -800000000000ll && value < 0x800000000000ll);
        if (value < 0x2000ll && value >= -0x2000ll)
            saveField(value + 0x2000ll, 0, 2);
        else if (value < 0x200000ll && value >= -0x200000ll)
            saveField(value + 0x200000ll, 1, 3);
        else if (value < 0x20000000ll && value >= -0x20000000ll)
            saveField(value + 0x20000000ll, 2, 4);
        else if (value < 0x2000000000ll-1 && value > -0x2000000000ll)
            saveField(value + 0x2000000000ll, 3, 5);
        else {
            saveField(0xffffffffffll, 3, 5);
            qint64 data = qToBigEndian((value +  0x800000000000ll) << 16);
            m_stream.write((&data), 6);
        }
    }

    void saveField(qint64 field, quint8 header, int dataLen)
    {
        field = qToBigEndian(field << (64 - dataLen*8));
        quint8* data = (quint8*) &field;
        data[0] &= 0x3f;
        data[0] |= (header << 6);
        m_stream.write(((const char*) data), dataLen);
    }

    void writeBytes(const QByteArray &value) {
        m_stream.write(value.data(), value.size());
    }

private:
    QnOutputBinaryStream<Output> m_stream;
    bool m_signed;
    qint64 m_lastValue;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnCompressedTimeWriter<Output> *disable_user_conversions(QnCompressedTimeWriter<Output> *value) {
    return value;
}


#endif // QN_COMPRESSED_TIME_WRITER_H
