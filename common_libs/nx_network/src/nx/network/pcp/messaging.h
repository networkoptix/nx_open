#ifndef PCP_MESSAGING_H
#define PCP_MESSAGING_H

#include <QtCore/QDataStream>

#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <netinet/in.h>
#endif

namespace pcp {

/** Port Control Protocol (PCP)
 *  for details: https://tools.ietf.org/html/rfc6887
 */
static const quint8 VERSION = 2;
static const int MAX_MESSAGE_SIZE = 1100;

static const quint16 CLIENT_PORT = 5350;
static const quint16 SERVER_PORT = 5351;

enum class Opcode : quint8
{
    ANNOUNCE = 0,
    MAP,
    PEER,

    INVALID = 0xFF
};

enum class ResultCode : quint8
{
    SUCCESS = 0,
    UNSUPP_VERSION,
    NOT_AUTHORIZED,
    MALFORMED_REQUEST,
    UNSUPP_OPCODE,
    UNSUPP_OPTION,
    MALFORMED_OPTION,
    NETWORK_FAILURE,
    NO_RESOURCES,
    UNSUPP_PROTOCOL,
    USER_EX_QUOTA,
    CANNOT_PROVIDE_EXTERNAL,
    ADDRESS_MISMATCH,
    EXCESSIVE_REMOTE_PEERS,
};

struct RequestHeader
{
    quint8      version;
    Opcode      opcode;

    quint32     lifeTime;
    QByteArray  clientIp;
};

QDataStream& operator<<(QDataStream& stream, const RequestHeader& data);
QDataStream& operator>>(QDataStream& stream, RequestHeader& data);

struct ResponseHeadeer
{
    quint8      version;
    Opcode      opcode;
    ResultCode  resultCode;

    quint32     lifeTime;
    quint32     epochTime;
};

QDataStream& operator<<(QDataStream& stream, const ResponseHeadeer& data);
QDataStream& operator>>(QDataStream& stream, ResponseHeadeer& data);

struct MapMessage
{
    QByteArray  nonce;
    quint8      protocol;

    quint16     internalPort;
    quint16     externalPort;
    QByteArray  externalIp;
};

QDataStream& operator<<(QDataStream& stream, const MapMessage& data);
QDataStream& operator>>(QDataStream& stream, MapMessage& data);

struct PeerMessage
{
    QByteArray  nonce;
    quint8      protocol;

    quint16     internalPort;
    quint16     externalPort;
    QByteArray  externalIp;

    quint16     remotePort;
    QByteArray  remoteIp;
};

QDataStream& operator<<(QDataStream& stream, const PeerMessage& data);
QDataStream& operator>>(QDataStream& stream, PeerMessage& data);

QByteArray makeRandomNonce();

} // namespace pcp

#endif // PCP_MESSAGING_H
