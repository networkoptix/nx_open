#include "api_peer_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>

namespace ec2 {

ec2::ApiPeerDataEx deserializeRemotePeerInfo(const nx_http::Request& request)
{
    ec2::ApiPeerDataEx remotePeer;
    QUrlQuery query(request.requestLine.url.query());

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem(QString::fromLatin1("format")))
        QnLexical::deserialize(query.queryItemValue(QString::fromLatin1("format")), &dataFormat);

    bool success = false;
    QByteArray peerData = nx_http::getHeaderValue(request.headers, Qn::EC2_PEER_DATA);
    peerData = QByteArray::fromBase64(peerData);
    if (dataFormat == Qn::JsonFormat)
        remotePeer = QJson::deserialized(peerData, ec2::ApiPeerDataEx(), &success);
    else if (dataFormat == Qn::UbjsonFormat)
        remotePeer = QnUbjson::deserialized(peerData, ec2::ApiPeerDataEx(), &success);

    if (remotePeer.id.isNull())
        remotePeer.id = QnUuid::createUuid();
    return remotePeer;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
(ApiPersistentIdData)(ApiPeerData)(ApiPeerDataEx), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
