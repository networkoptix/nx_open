#ifndef QN_COMPRESSED_TIME_READER_H
#define QN_COMPRESSED_TIME_READER_H

#include <cassert>

#include <algorithm> /* For std::min. */

#include <QtCore/QtGlobal>
#include <QtCore/QVarLengthArray>
#include <QtCore/QtEndian>

#include "binary_stream.h"
#include "compressed_time_fwd.h"
#include "recording/time_period.h"

#include <nx/utils/log/assert.h>

template<class Input>
class QnCompressedTimeReader {
public:
    QnCompressedTimeReader(const Input *data):
        m_stream(data),
        m_lastValue(0)
    {
        char format[3];
        memset(format, 0, sizeof(format));
        m_stream.read(format, sizeof(format));
        m_signed = memcmp(format, QnCompressedTime::SIGNED_FORMAT, sizeof(QnCompressedTime::SIGNED_FORMAT)) == 0;
    }

    void resetLastValue() {
        m_lastValue = 0;
    }

    /*
    bool readInt64(qint64 *target) {
        qint64 deltaVal = 0;
        bool rez = m_signed ? decodeSignedNumber(deltaVal) : decodeUnsignedNumber(deltaVal);
        if (rez) {
            m_lastValue += deltaVal;
            *target = m_lastValue;
        }
        return rez;
    }
    */
    bool readQnTimePeriod(QnTimePeriod* value)
    {
        bool rez = m_signed ? decodeSignedNumber(value->startTimeMs) : decodeUnsignedNumber(value->startTimeMs);
        if (!rez)
            return rez;
        value->startTimeMs += m_lastValue;
        if (!decodeUnsignedNumber(value->durationMs))
            return false;
        --value->durationMs;
        m_lastValue = value->startTimeMs + value->durationMs;
        return true;
    }

    bool readQnUuid(QnUuid* value)
    {
        char tmp[16];
        if (m_stream.read(tmp, sizeof(tmp)) != sizeof(tmp))
            return false;
        *value = QnUuid::fromRfc4122(QByteArray::fromRawData(tmp, sizeof(tmp)));
        return true;
    }

    int pos() const { return m_stream.pos(); }
private:
    bool decodeSignedNumber(qint64& result) {
        int decoded = decodeValue(result);
        if (decoded <= 0)
            return false;
        NX_ASSERT((decoded >= 2 && decoded <= 5) || decoded == 11);
        qint64 offset = 0x20ll << ((decoded-1) * 8);
        if (decoded == 11)
            offset = 0x800000000000ll;
        result -= offset;
        return true;
    }

    bool decodeUnsignedNumber(qint64& result) {
        return decodeValue(result) > 0;
    }


    int decodeValue(qint64& value)
    {
        quint8 tmp[8];
        if (m_stream.read(&tmp, 1) != 1)
            return -1;
        int fieldSize = 2 + (tmp[0] >> 6);
        if (m_stream.read(&tmp[1], fieldSize-1) != fieldSize-1)
            return -1;
        value = tmp[0] & 0x3f;
        for (int i = 1; i < fieldSize; ++i)
            value = (value << 8) + tmp[i];

        if (value == 0x3fffffffffll)
        {
            if (m_stream.read(&tmp, 6) != 6)
                return -1;
            fieldSize += 6;
            value = tmp[0];
            for (int i = 1; i < 6; ++i)
                value = (value << 8) + tmp[i];
        }
        return fieldSize;
    };

    template<class T>
    bool readNumber(T *target) {
        if(m_stream.read(target, sizeof(T)) != sizeof(T))
            return false;

        *target = qFromBigEndian(*target);
        return true;
    }
public:
    bool readSizeFromStream(int *target)
    {
        return readNumber(target);
    }
private:
    QnInputBinaryStream<Input> m_stream;
    bool m_signed;
    qint64 m_lastValue;
};


/* Disable conversion wrapping for stream types as they are not convertible to anything
 * anyway. Also when wrapping is enabled, ADL fails to find template overloads. */

template<class Output>
QnCompressedTimeReader<Output> *disable_user_conversions(QnCompressedTimeReader<Output> *value) {
    return value;
}


#endif // QN_COMPRESSED_TIME_READER_H
