/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#include "networkoptixmodulerevealcommon.h"

#include <QtCore/QMap>
#include <QtCore/QJsonDocument>

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

bool RevealRequest::deserialize(const quint8 **bufStart, const quint8 *bufEnd) {
    if (bufEnd - *bufStart < revealRequestStr.size())
        return false;

    if (memcmp(*bufStart, revealRequestStr.data(), revealRequestStr.size()) != 0)
        return false;

    *bufStart += revealRequestStr.size();
    return true;
}



RevealResponse::RevealResponse() : port(0), sslAllowed(false) {}

RevealResponse::RevealResponse(const QnModuleInformation &moduleInformation) {
    type = moduleInformation.type;
    version = moduleInformation.version.toString();
    systemInformation = moduleInformation.systemInformation.toString();
    customization = moduleInformation.customization;
    name = moduleInformation.systemName;
    moduleName = moduleInformation.name;
    seed = moduleInformation.id;
    port = moduleInformation.port;
    sslAllowed = moduleInformation.sslAllowed;
    remoteAddresses = moduleInformation.remoteAddresses.toList();
    authHash = moduleInformation.authHash;
}

QnModuleInformation RevealResponse::toModuleInformation() const {
    QnModuleInformation moduleInformation;
    moduleInformation.type = type;
    moduleInformation.version = QnSoftwareVersion(version);
    moduleInformation.systemInformation = QnSystemInformation(systemInformation);
    moduleInformation.systemName = name;
    moduleInformation.name = moduleName;
    moduleInformation.id = seed;
    moduleInformation.port = port;
    moduleInformation.remoteAddresses = QSet<QString>::fromList(remoteAddresses);
    moduleInformation.sslAllowed = sslAllowed;
    moduleInformation.authHash = authHash;
    return moduleInformation;
}

bool RevealResponse::serialize(quint8 **const bufStart, const quint8 *bufEnd) {
    QVariantMap map;
    map[lit("application")] = type;
    map[lit("version")] = version;
    map[lit("customization")] = customization;
    map[lit("seed")] = seed.toString();
    map[lit("systemName")] = name;
    map[lit("name")] = moduleName;
    map[lit("systemInformation")] = systemInformation;
    map[lit("sslAllowed")] = sslAllowed;
    map[lit("port")] = port;
    map[lit("remoteAddresses")] = remoteAddresses;
    map[lit("authHash")] = authHash;

    QByteArray data = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
    if (data.size() > bufEnd - *bufStart)
        return false;
    memcpy(*bufStart, data.data(), data.size());
    *bufStart += data.size();
    return true;
}

bool RevealResponse::deserialize(const quint8 **bufStart, const quint8 *bufEnd) {
    while (*bufStart < bufEnd && **bufStart != '{')
        (*bufStart)++;

    QByteArray data(reinterpret_cast<const char *>(*bufStart), bufEnd - *bufStart);

    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    type = map.take(lit("application")).toString();
    version = map.take(lit("version")).toString();
    systemInformation = map.take(lit("systemInformation")).toString();
    customization = map.take(lit("customization")).toString();
    name = map.take(lit("systemName")).toString();
    moduleName = map.take(lit("name")).toString();
    seed = QUuid(map.take(lit("seed")).toString());
    sslAllowed = map.take( lit( "sslAllowed" ) ).toBool();
    port = static_cast<quint16>(map.take(lit("port")).toUInt());
    remoteAddresses = map.take(lit("remoteAddresses")).toStringList();
    authHash = map.take(lit("authHash")).toByteArray();

    return !type.isEmpty() && !version.isEmpty();
}
