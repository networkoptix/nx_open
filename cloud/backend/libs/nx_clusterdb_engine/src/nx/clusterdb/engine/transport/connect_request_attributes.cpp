#include "connect_request_attributes.h"

#include <QtCore/QUrlQuery>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>

#include <nx/vms/api/types/connection_types.h>

namespace nx::clusterdb::engine::transport {

static bool readCommonAttrs(
    const network::http::HttpHeaders& headers,
    ConnectionRequestAttributes* value)
{
    auto connectionIdIter = headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == headers.end())
        return false;
    value->connectionId = connectionIdIter->second.toStdString();

    if (!nx::network::http::readHeader(
            headers,
            Qn::EC2_PROTO_VERSION_HEADER_NAME,
            &value->remotePeerProtocolVersion))
    {
        NX_VERBOSE(typeid(ConnectionRequestAttributes),
            "Missing required header %1", Qn::EC2_PROTO_VERSION_HEADER_NAME);
        return false;
    }

    // Checking content encoding requested by remote peer.
    auto acceptEncodingHeaderIter = headers.find("Accept-Encoding");
    if (acceptEncodingHeaderIter != headers.end())
    {
        nx::network::http::header::AcceptEncodingHeader acceptEncodingHeader(
            acceptEncodingHeaderIter->second);
        if (acceptEncodingHeader.encodingIsAllowed("identity"))
            value->contentEncoding = "identity";
        else if (acceptEncodingHeader.encodingIsAllowed("gzip"))
            value->contentEncoding = "gzip";
    }

    value->nodeName = nx::network::http::getHeaderValue(headers, "User-Agent").toStdString();
    if (value->nodeName.empty())
        value->nodeName = nx::network::http::getHeaderValue(headers, "Server").toStdString();

    return true;
}

//-------------------------------------------------------------------------------------------------

void ConnectionRequestAttributes::write(network::http::HttpHeaders* headers)
{
    headers->emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, connectionId.c_str());
    headers->emplace(Qn::EC2_PROTO_VERSION_HEADER_NAME, std::to_string(remotePeerProtocolVersion).c_str());
    headers->emplace(Qn::PEER_GUID_HEADER_NAME, remotePeer.id.toSimpleByteArray());
    headers->emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, remotePeer.instanceId.toSimpleByteArray());
}

bool ConnectionRequestAttributes::read(const network::http::HttpHeaders& headers)
{
    if (!readCommonAttrs(headers, this))
        return false;

    auto peerIdIter = headers.find(Qn::PEER_GUID_HEADER_NAME);
    if (peerIdIter == headers.end())
        return false;
    const auto peerId = QnUuid::fromStringSafe(peerIdIter->second);

    auto peerRuntimeIdIter = headers.find(Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    if (peerRuntimeIdIter == headers.end())
        return false;
    const auto peerRuntimeId = QnUuid::fromStringSafe(peerRuntimeIdIter->second);

    remotePeer = vms::api::PeerData(
        peerId,
        peerRuntimeId,
        vms::api::PeerType::server,
        Qn::UbjsonFormat);

    return true;
}

//-------------------------------------------------------------------------------------------------

bool fetchDataFromConnectRequest(
    const nx::network::http::Request& request,
    ConnectionRequestAttributes* connectionRequestAttributes)
{
    if (!readCommonAttrs(request.headers, connectionRequestAttributes))
        return false;

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

    connectionRequestAttributes->remotePeer =
        vms::api::PeerData(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    return true;
}

} // namespace nx::clusterdb::engine::transport
