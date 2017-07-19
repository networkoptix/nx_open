#include "module_information.h"

#include <QtNetwork/QNetworkInterface>
#include <QtCore/QCryptographicHash>

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>

#include <nx_ec/ec_proto_version.h>

namespace {
/*!
This string represents client during search with NetworkOptixModuleFinder class.
It may look strange, but "client.exe" is valid on linux too (VER_ORIGINALFILENAME_STR from app_info.h)
*/
const QString nxClientId = lit("client.exe");
const QString nxECId = lit("Enterprise Controller");
const QString nxMediaServerId = lit("Media Server");
} // namespace

QnModuleInformation::QnModuleInformation():
    type(),
    customization(),
    version(),
    systemInformation(),
    systemName(),
    name(),
    port(0),
    id(),
    sslAllowed(false),
    protoVersion(0),
    runtimeId(),
    serverFlags(0),
    realm(nx::network::AppInfo::realm()),
    ecDbReadOnly(false),
    cloudSystemId(),
    cloudHost(nx::network::AppInfo::defaultCloudHost())
{
}

void QnModuleInformation::fixRuntimeId()
{
    if (!runtimeId.isNull())
        return;
    const int bytesNeeded = 16;

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(id.toRfc4122());
    md5.addData(customization.toLatin1());
    md5.addData(systemName.toLatin1());
    md5.addData(QByteArray::number(port));

    QByteArray hash = md5.result();
    while (hash.size() < bytesNeeded)
        hash += hash;
    hash.resize(16);
    runtimeId = QnUuid::fromRfc4122(hash);
}

QString QnModuleInformation::cloudId() const
{
    if (!cloudSystemId.isEmpty())
        return id.toSimpleString() + lit(".") + cloudSystemId;

    return QString();
}

QString QnModuleInformation::nxMediaServerId() {
    return ::nxMediaServerId;
}

QString QnModuleInformation::nxECId() {
    return ::nxECId;
}

QString QnModuleInformation::nxClientId() {
    return ::nxClientId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnModuleInformation)(QnModuleInformationWithAddresses), (ubjson)(xml)(json)(eq), _Fields)
