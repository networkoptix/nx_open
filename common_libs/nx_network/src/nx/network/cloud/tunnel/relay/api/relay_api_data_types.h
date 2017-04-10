#pragma once

#include <chrono>
#include <string>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

struct BeginListeningRequest
{
    std::string peerName;
};

#define BeginListeningRequest_Fields (peerName)

struct BeginListeningResponse
{
    int preemptiveConnectionCount = 0;
};

#define BeginListeningResponse_Fields (preemptiveConnectionCount)

struct CreateClientSessionRequest
{
    std::string targetPeerName;
    std::string desiredSessionId;
};

#define CreateClientSessionRequest_Fields (targetPeerName)(desiredSessionId)

struct CreateClientSessionResponse
{
    std::string sessionId;
    std::chrono::seconds sessionTimeout;
};

#define CreateClientSessionResponse_Fields (sessionId)(sessionTimeout)

struct ConnectToPeerRequest
{
    std::string sessionId;
};

#define ConnectToPeerRequest_Fields (sessionId)

NX_NETWORK_API bool deserialize(QnJsonContext*, const QJsonValue&, BeginListeningRequest*);
NX_NETWORK_API void serialize(QnJsonContext*, const BeginListeningRequest&, QJsonValue*);

NX_NETWORK_API bool deserialize(QnJsonContext*, const QJsonValue&, BeginListeningResponse*);
NX_NETWORK_API void serialize(QnJsonContext*, const BeginListeningResponse&, QJsonValue*);

NX_NETWORK_API bool deserialize(QnJsonContext*, const QJsonValue&, CreateClientSessionRequest*);
NX_NETWORK_API void serialize(QnJsonContext*, const CreateClientSessionRequest&, QJsonValue*);

NX_NETWORK_API bool deserialize(QnJsonContext*, const QJsonValue&, CreateClientSessionResponse*);
NX_NETWORK_API void serialize(QnJsonContext*, const CreateClientSessionResponse&, QJsonValue*);

NX_NETWORK_API bool deserialize(QnJsonContext*, const QJsonValue&, ConnectToPeerRequest*);
NX_NETWORK_API void serialize(QnJsonContext*, const ConnectToPeerRequest&, QJsonValue*);

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
