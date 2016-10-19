#include "module_information.h"

#include <QtNetwork/QNetworkInterface>

#include <nx/fusion/model_functions.h>
#include <common/common_module.h>
#include <nx_ec/ec_proto_version.h>
#include <api/model/connection_info.h>
#include <utils/common/app_info.h>
#include <utils/common/software_version.h>

namespace {
/*!
This string represents client during search with NetworkOptixModuleFinder class.
It may look strange, but "client.exe" is valid on linux too (VER_ORIGINALFILENAME_STR from app_info.h)
*/
const QString nxClientId = lit("client.exe");
const QString nxECId = lit("Enterprise Controller");
const QString nxMediaServerId = lit("Media Server");

static const auto kMinVersionWithSystem = QnSoftwareVersion(2, 3);
static const auto kMinVersionWithLocalId = QnSoftwareVersion(3, 0);

QString getFactorySystemIdImpl(const QnUuid& serverId)
{
    return serverId.toString();
}

QString getTargetSystemIdImpl(
    const QString& systemName,
    const QnUuid& localSystemId,
    const QnUuid& serverId,
    const QnSoftwareVersion& serverVersion)
{
    if (serverVersion < kMinVersionWithSystem)
        return serverId.toString();    //< We have only one hub-server if version is less than 2.3

    if (serverVersion < kMinVersionWithLocalId)
        return systemName; //< No cloud, no local id, no new systems

    if (localSystemId.isNull())
        return getFactorySystemIdImpl(serverId);  //< New System id

    return localSystemId.toString();
}

}

QString helpers::getTargetSystemId(const QnConnectionInfo& info)
{
    return ::getTargetSystemIdImpl(info.systemName, info.localSystemId,
        info.serverId(), info.version);
}

QString helpers::getTargetSystemId(const QnModuleInformation& info)
{
    return ::getTargetSystemIdImpl(info.systemName, info.localSystemId,
        info.id, info.version);
}

QString helpers::getFactorySystemId(const QnModuleInformation& info)
{
    return getFactorySystemIdImpl(info.id);
}

bool helpers::isNewSystem(const QnModuleInformation& info)
{
    if (info.version < kMinVersionWithLocalId)
        return false; //< No new systems until 3.0

    return info.localSystemId.isNull();
}

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
    realm(QnAppInfo::realm()),
    ecDbReadOnly(false),
    cloudSystemId(),
    cloudHost(QnAppInfo::defaultCloudHost())
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
