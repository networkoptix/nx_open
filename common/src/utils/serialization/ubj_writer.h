#ifndef QN_UBJ_WRITER_H
#define QN_UBJ_WRITER_H

#include "binary_stream.h"
#include "ubj_writer.h"

template<class T>
class QnUbjWriter {
public:
    QnUbjWriter(QnOutputBinaryStream<T> *stream): 
        m_stream(stream)
    {}

    void writeNull() {
        writeMarkerInternal(QnUbj::NullMarker);
    }

    void writeBool(bool value) {
        writeMarkerInternal(value ? QnUbj::TrueMarker : QnUbj::FalseMarker);
    }

    void writeUInt8(std::uint8_t value) {
        return writeNumberInternal(QnUbj::UInt8Marker, value);
    }

    void writeInt8(std::int8_t value) {
        return writeNumberInternal(QnUbj::Int8Marker, value);
    }

    void writeInt16(std::int16_t value) {
        return writeNumberInternal(QnUbj::Int16Marker, value);
    }

    void writeInt32(std::int32_t value) {
        return writeNumberInternal(QnUbj::Int32Marker, value);
    }

    void writeInt64(std::int64_t value) {
        return writeNumberInternal(QnUbj::Int64Marker, value);
    }

    void writeFloat(float value) {
        return writeNumberInternal(QnUbj::FloatMarker, value);
    }

    void writeDouble(double value) {
        return writeNumberInternal(QnUbj::DoubleMarker, value);
    }

    void writeBigNumber(QByteArray value) {
        return writeUtf8StringInternal(QnUbj::BigNumberMarker, value);
    }

    void writeLatin1Char(char value) {
        return writeNumberInternal(QnUbj::Latin1CharMarker, value);
    }

    void writeUtf8String(QByteArray value) {
        return writeUtf8StringInternal(QnUbj::Utf8StringMarker, value);
    }

    void writeUtf8String(QString value) {
        writeUtf8String(value.toUtf8());
    }

private:
    template<class Number>
    void writeNumberInternal(QnUbj::Marker marker, Number value) {
        writeMarkerInternal(marker);

        value = toBigEndian(value);
        m_stream->write(&value, sizeof(Number));
    }

    void writeUtf8StringInternal(QnUbj::Marker marker, const QByteArray &value) {
        writeMarkerInternal(marker);
        writeInt32(value.size()); // TODO: #Elric can save here by adaptively using int8/int16
        writeBytesInternal(value);
    }

    void writeBytesInternal(QnUbj::Marker marker, const QByteArray &value) {
        return m_stream->write(value.data(), value.size());
    }

    void writeMarkerInternal(QnUbj::Marker marker) {
        writeMarkerToStream(marker);
    }

    void writeMarkerToStream(QnUbj::Marker marker) {
        char c = QnUbj::charFromMarker(marker);
        m_stream->write(&c, 1);
    }

    template<class Number>
    static Number toBigEndian(const Number &value) {
        return qToBigEndian(value);
    }

    static char toBigEndian(char value) {
        return value;
    }

    static float toBigEndian(float value) {
        std::uint32_t tmp = reinterpret_cast<std::uint32_t &>(value);
        tmp = qToBigEndian(tmp);
        return reinterpret_cast<float &>(tmp);
    }

    static double toBigEndian(double value) {
        std::uint64_t tmp = reinterpret_cast<std::uint64_t &>(value);
        tmp = qToBigEndian(tmp);
        return reinterpret_cast<double &>(tmp);
    }


private:
    QnOutputBinaryStream<T> *m_stream;
}


#endif // QN_UBJ_WRITER_H
