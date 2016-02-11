#include "messaging.h"

#include "utils/memory/data_stream_helpers.h"

static const int NONCE_SIZE = 12;
static const int IP_SIZE = 16;
static const quint8 RESPONSE_BIT = (1 << 7);

namespace pcp {

QDataStream& operator<<(QDataStream& stream, const RequestHeader& data)
{
    quint16 r = 0;
    quint8 opcode = static_cast<quint8>(data.opcode);

    stream << data.version << opcode << r << data.lifeTime;
    dsh::write(stream, data.clientIp, IP_SIZE);

    return stream;
}

QDataStream& operator>>(QDataStream& stream, RequestHeader& data)
{
    quint16 r;
    quint8 opcode;

    stream >> data.version >> opcode >> r >> data.lifeTime;
    dsh::read(stream, data.clientIp, IP_SIZE);

    data.opcode = static_cast<Opcode>(opcode);
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const ResponseHeadeer& data)
{
    quint8 r8 = 0;
    quint32 r32 = 0;

    quint8 opcode = static_cast<quint8>(data.opcode) | RESPONSE_BIT;
    quint8 result = static_cast<quint8>(data.resultCode);

    stream << data.version << opcode << r8 << result
           << data.lifeTime << data.epochTime << r32 << r32 << r32;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, ResponseHeadeer& data)
{
    quint8 r8;
    quint32 r32;

    quint8 opcode;
    quint8 result;

    stream >> data.version >> opcode >> r8 >> result
           >> data.lifeTime >> data.epochTime >> r32 >> r32 >> r32;

    data.opcode = (opcode & RESPONSE_BIT) ? static_cast<Opcode>(opcode ^ RESPONSE_BIT) : Opcode::INVALID;
    data.resultCode = static_cast<ResultCode>(result);
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const MapMessage& data)
{
    quint8 r8 = 0;
    quint16 r16 = 0;

    dsh::write(stream, data.nonce, NONCE_SIZE);
    stream << data.protocol << r8 << r16
           << data.internalPort << data.externalPort;
    dsh::write(stream, data.externalIp, IP_SIZE);

    return stream;
}

QDataStream& operator>>(QDataStream& stream, MapMessage& data)
{
    quint8 r8;
    quint16 r16;

    dsh::read(stream, data.nonce, NONCE_SIZE);
    stream >> data.protocol >> r8 >> r16
           >> data.internalPort >> data.externalPort;
    dsh::read(stream, data.externalIp, IP_SIZE);

    return stream;
}

QDataStream& operator<<(QDataStream& stream, const PeerMessage& data)
{
    quint8 r8 = 0;
    quint16 r16 = 0;

    dsh::write(stream, data.nonce, NONCE_SIZE);
    stream << data.protocol << r8 << r16
           << data.internalPort << data.externalPort;
    dsh::write(stream, data.externalIp, IP_SIZE);

    stream << data.remotePort << r16;
    dsh::write(stream, data.remoteIp, IP_SIZE);

    return stream;
}

QDataStream& operator>>(QDataStream& stream, PeerMessage& data)
{
    quint8 r8;
    quint16 r16;

    dsh::read(stream, data.nonce, NONCE_SIZE);
    stream >> data.protocol >> r8 >> r16
           >> data.internalPort >> data.externalPort;
    dsh::read(stream, data.externalIp, IP_SIZE);

    stream >> data.remotePort >> r16;
    dsh::read(stream, data.remoteIp, IP_SIZE);

    return stream;
}

QByteArray makeRandomNonce()
{
    QByteArray nonce(NONCE_SIZE, Qt::Uninitialized);
    for (auto& b : nonce)
        b = static_cast<char>(qrand());
    return nonce;
}

} // namespace pcp
