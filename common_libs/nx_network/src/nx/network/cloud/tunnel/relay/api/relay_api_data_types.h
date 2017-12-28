#pragma once

#include <chrono>
#include <string>

#include <boost/optional.hpp>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/network/abstract_socket.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

struct BeginListeningRequest
{
    std::string peerName;
};

#define BeginListeningRequest_Fields (peerName)

struct NX_NETWORK_API BeginListeningResponse
{
    int preemptiveConnectionCount;
    boost::optional<network::KeepAliveOptions> keepAliveOptions;

    BeginListeningResponse();

    bool operator==(const BeginListeningResponse& right) const;
};

NX_NETWORK_API bool serializeToHeaders(
    nx::network::http::HttpHeaders* where,
    const BeginListeningResponse& what);

NX_NETWORK_API bool deserializeFromHeaders(
    const nx::network::http::HttpHeaders& from,
    BeginListeningResponse* what);

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
    /**
     * This url can be different from the initial one in case of redirect to another relay instance.
     */
    std::string actualRelayUrl;
};

#define CreateClientSessionResponse_Fields (sessionId)(sessionTimeout)

NX_NETWORK_API bool serializeToHeaders(
    nx::network::http::HttpHeaders* where,
    const CreateClientSessionResponse& what);

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
