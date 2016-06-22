#include "proto_message.h"

#include <QtCore/QtEndian>

#include "binary_stream.h"


template<bool acceptEmpty, class T>
static int readVariant(QnInputBinaryStream<QByteArray> &stream, T *target, QnProtoParseError *error) {
    T result = 0;
    int shift = 0;

    while(true) {
        quint8 byte;
        if(stream.read(&byte, 1) != 1) {
            if(acceptEmpty && shift == 0)
                return 0;

            if(error)
                *error = QnProtoParseError(stream.pos(), QnProtoParseError::UnexpectedTermination);
            return -1;
        }

        result |= (byte & 0x7F) << shift;
        if(!(byte & 0x80))
            break;

        shift += 7;
        if(shift >= sizeof(T) * 8 + 7) {
            if(error)
                *error = QnProtoParseError(stream.pos(), QnProtoParseError::VariantTooLong);
            return -1;
        }
    }

    *target = result;
    return shift / 7 + 1;
}

QnProtoMessage QnProtoMessage::fromProto(const QByteArray &proto, QnProtoParseError *error) {
    QnInputBinaryStream<QByteArray> stream(&proto);

    QnProtoMessage result;

    while(true) {
        quint32 marker;
        int read = readVariant<true>(stream, &marker, error);
        if(read == 0) {
            return result;
        } else if(read == -1) {
            return QnProtoMessage();
        }

        QnProtoValue::Type type = static_cast<QnProtoValue::Type>(marker & 0x07);
        int index = static_cast<int>(marker >> 3);

        switch (type) {
        case QnProtoValue::Variant:
            quint64 variant;
            if(readVariant<false>(stream, &variant, error) == -1)
                return QnProtoMessage();

            result.push_back(QnProtoRecord(index, QnProtoValue(variant, QnProtoValue::UInt64Variant)));
            break;
        case QnProtoValue::Fixed64: {
            quint64 fixed64;
            if(stream.read(&fixed64, 8) != 8) {
                if(error)
                    *error = QnProtoParseError(stream.pos(), QnProtoParseError::UnexpectedTermination);
                return QnProtoMessage();
            }
            fixed64 = qFromLittleEndian(fixed64);

            result.push_back(QnProtoRecord(index, QnProtoValue(fixed64)));
            break;
        }
        case QnProtoValue::Fixed32: {
            quint32 fixed32;
            if(stream.read(&fixed32, 4) != 4) {
                if(error)
                    *error = QnProtoParseError(stream.pos(), QnProtoParseError::UnexpectedTermination);
                return QnProtoMessage();
            }
            fixed32 = qFromLittleEndian(fixed32);

            result.push_back(QnProtoRecord(index, QnProtoValue(fixed32)));
            break;
        }
        case QnProtoValue::Bytes: {
            qint32 size;
            if(readVariant<false>(stream, &size, error) == -1)
                return QnProtoMessage();

            if(size > proto.size() || size < 0) {
                if(error)
                    *error = QnProtoParseError(stream.pos(), QnProtoParseError::UnexpectedTermination);
                return QnProtoMessage();
            }

            QByteArray bytes(size, Qt::Uninitialized);
            if(stream.read(bytes.data(), size) != size) {
                if(error)
                    *error = QnProtoParseError(stream.pos(), QnProtoParseError::UnexpectedTermination);
                return QnProtoMessage();
            }

            result.push_back(QnProtoRecord(index, QnProtoValue(bytes)));
            break;
        }
        default:
            if(error)
                *error = QnProtoParseError(stream.pos() - 1, QnProtoParseError::InvalidMarker);
            return QnProtoMessage();
        }
    }

    return result;
}
