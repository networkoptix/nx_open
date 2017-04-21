#pragma once

namespace ec2 {

enum class MessageType
{
    resolvePeerNumberRequest,
    resolvePeerNumberResponse,
    alivePeers,
    subscribeForDataUpdates,
    pushTransactionData
};

static const quint32 kMaxDistance = std::numeric_limits<quint32>::max();
static const char* kP2pProtoName = "p2p";

} // namespace ec2
