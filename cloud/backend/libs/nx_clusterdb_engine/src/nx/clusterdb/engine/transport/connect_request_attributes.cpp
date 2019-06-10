#include "connect_request_attributes.h"

#include <QtCore/QUrlQuery>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>

#include <nx/vms/api/types/connection_types.h>

namespace nx::clusterdb::engine::transport {

bool fetchDataFromConnectRequest(
    const nx::network::http::Request& request,
    ConnectionRequestAttributes* connectionRequestAttributes)
{
    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return false;
    connectionRequestAttributes->connectionId = connectionIdIter->second.toStdString();

    if (!nx::network::http::readHeader(
        request.headers,
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        &connectionRequestAttributes->remotePeerProtocolVersion))
    {
        NX_VERBOSE(typeid(ConnectionRequestAttributes),
            lm("Missing required header %1").arg(Qn::EC2_PROTO_VERSION_HEADER_NAME));
        return false;
    }

    QUrlQuery query = QUrlQuery(request.requestLine.url.query());
    const bool isClient = query.hasQueryItem("isClient");
    const bool isMobileClient = query.hasQueryItem("isMobile");
    QnUuid remoteGuid = QnUuid::fromStringSafe(query.queryItemValue("guid"));
    QnUuid remoteRuntimeGuid = QnUuid::fromStringSafe(query.queryItemValue("runtime-guid"));
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();

    const vms::api::PeerType peerType = isMobileClient
        ? vms::api::PeerType::mobileClient
        : (isClient ? vms::api::PeerType::desktopClient : vms::api::PeerType::server);

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
    {
        if (!QnLexical::deserialize(query.queryItemValue("format"), &dataFormat))
        {
            NX_DEBUG(typeid(ConnectionRequestAttributes),
                lm("Invalid value of \"format\" field: %1")
                .arg(query.queryItemValue("format")));
            return false;
        }
    }

    // Checking content encoding requested by remote peer.
    auto acceptEncodingHeaderIter = request.headers.find("Accept-Encoding");
    if (acceptEncodingHeaderIter != request.headers.end())
    {
        nx::network::http::header::AcceptEncodingHeader acceptEncodingHeader(
            acceptEncodingHeaderIter->second);
        if (acceptEncodingHeader.encodingIsAllowed("identity"))
            connectionRequestAttributes->contentEncoding = "identity";
        else if (acceptEncodingHeader.encodingIsAllowed("gzip"))
            connectionRequestAttributes->contentEncoding = "gzip";
    }

    connectionRequestAttributes->remotePeer =
        vms::api::PeerData(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    return true;
}

} // namespace nx::clusterdb::engine::transport
