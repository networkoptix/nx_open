#include "messaging.h"

#include <nx/utils/random.h>

static const int NONCE_SIZE = 12;
static const int IP_SIZE = 16;
static const quint8 RESPONSE_BIT = (1 << 7);

namespace nx {
namespace network {
namespace pcp {

QDataStream& operator<<(QDataStream& stream, const RequestHeader& data)
{
    quint16 r = 0;
    quint8 opcode = static_cast<quint8>(data.opcode);

    stream << data.version << opcode << r << data.lifeTime;
    stream.writeRawData(data.clientIp.data(), data.clientIp.size());

    return stream;
}

QDataStream& operator>>(QDataStream& stream, RequestHeader& data)
{
    quint16 r;
    quint8 opcode;

    stream >> data.version >> opcode >> r >> data.lifeTime;
    data.clientIp.resize(IP_SIZE);
    stream.readRawData(data.clientIp.data(), IP_SIZE);

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

    stream.writeRawData(data.nonce.data(), data.nonce.size());
    stream << data.protocol << r8 << r16 << data.internalPort << data.externalPort;
    stream.writeRawData(data.externalIp.data(), data.externalIp.size());

    return stream;
}

QDataStream& operator>>(QDataStream& stream, MapMessage& data)
{
    quint8 r8;
    quint16 r16;

    data.nonce.resize(NONCE_SIZE);
    stream.readRawData(data.nonce.data(), data.nonce.size());

    stream >> data.protocol >> r8 >> r16 >> data.internalPort >> data.externalPort;

    data.externalIp.resize(IP_SIZE);
    stream.readRawData(data.externalIp.data(), data.externalIp.size());

    return stream;
}

QDataStream& operator<<(QDataStream& stream, const PeerMessage& data)
{
    quint8 r8 = 0;
    quint16 r16 = 0;

    stream.writeRawData(data.nonce.data(), data.nonce.size());
    stream << data.protocol << r8 << r16 << data.internalPort << data.externalPort;
    stream.writeRawData(data.externalIp.data(), data.externalIp.size());
    stream << data.remotePort << r16;
    stream.writeRawData(data.remoteIp.data(), data.remoteIp.size());

    return stream;
}

QDataStream& operator>>(QDataStream& stream, PeerMessage& data)
{
    quint8 r8;
    quint16 r16;

    data.nonce.resize(NONCE_SIZE);
    stream.readRawData(data.nonce.data(), data.nonce.size());

    stream >> data.protocol >> r8 >> r16 >> data.internalPort >> data.externalPort;

    data.externalIp.resize(IP_SIZE);
    stream.readRawData(data.externalIp.data(), data.externalIp.size());

    stream >> data.remotePort >> r16;

    data.remoteIp.resize(IP_SIZE);
    stream.readRawData(data.remoteIp.data(), data.remoteIp.size());

    return stream;
}

QByteArray makeRandomNonce()
{
    return nx::utils::random::generate(NONCE_SIZE);
}

} // namespace pcp
} // namespace network
} // namespace nx
