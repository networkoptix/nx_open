/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#include "networkoptixmodulerevealcommon.h"

#include <QtCore/QMap>
#include <QtCore/QJsonDocument>

#include <nx_ec/ec_proto_version.h>

namespace {
    const QByteArray revealRequestStr("{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }");
}

bool RevealRequest::serialize(quint8 ** const bufStart, const quint8 *bufEnd) {
    if (bufEnd - *bufStart < revealRequestStr.size())
        return false;

    memcpy(*bufStart, revealRequestStr.data(), revealRequestStr.size());
    *bufStart += revealRequestStr.size();
    return true;
}

bool RevealRequest::isValid(const quint8 *bufStart, const quint8 *bufEnd) {
    if (bufEnd - bufStart < revealRequestStr.size())
        return false;

    if (memcmp(bufStart, revealRequestStr.data(), revealRequestStr.size()) != 0)
        return false;

    return true;
}


RevealResponse::RevealResponse(const QnModuleInformationEx &other)
    : QnModuleInformationEx(other)
{
}

QByteArray RevealResponse::serialize() {
    QVariantMap map;
    map[lit("application")] = type;
    map[lit("version")] = version.toString();
    map[lit("customization")] = customization;
    map[lit("seed")] = id.toString();
    map[lit("systemName")] = systemName;
    map[lit("name")] = name;
    map[lit("systemInformation")] = systemInformation.toString();
    map[lit("sslAllowed")] = sslAllowed;
    map[lit("port")] = port;
    map[lit("remoteAddresses")] = QStringList(remoteAddresses.toList());
    map[lit("authHash")] = authHash.toBase64();
    map[lit("protoVersion")] = protoVersion;
    map[lit("runtimeId")] = runtimeId.toString();
    return QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
}

bool RevealResponse::deserialize(const quint8 *bufStart, const quint8 *bufEnd) {
    while (bufStart < bufEnd && *bufStart != '{')
        bufStart++;

    QByteArray data(reinterpret_cast<const char *>(bufStart), bufEnd - bufStart);

    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    type = map.value(lit("application")).toString();
    version = QnSoftwareVersion(map.value(lit("version")).toString());
    systemInformation = map.value(lit("systemInformation")).toString();
    customization = map.value(lit("customization")).toString();
    systemName = map.value(lit("systemName")).toString();
    name = map.value(lit("name")).toString();
    id = QnUuid::fromStringSafe(map.value(lit("seed")).toString());
    sslAllowed = map.value(lit("sslAllowed")).toBool();
    port = static_cast<quint16>(map.value(lit("port")).toUInt());
//    remoteAddresses = map.value(lit("remoteAddresses")).toStringList();
    authHash = QByteArray::fromBase64(map.value(lit("authHash")).toByteArray());
    protoVersion = map.value(lit("protoVersion"), nx_ec::INITIAL_EC2_PROTO_VERSION).toInt();
    runtimeId = QnUuid::fromStringSafe(map.value(lit("runtimeId")).toString());

    return !type.isEmpty() && !version.isNull();
}
