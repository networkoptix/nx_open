#include "system_helpers.h"

#include <core/resource/media_server_resource.h>

#include <network/cloud_system_data.h>
#include <network/module_information.h>
#include <api/model/connection_info.h>
#include <api/global_settings.h>
#include <utils/common/id.h>
#include <common/common_module.h>

namespace {

static const auto kMinVersionWithSystem = QnSoftwareVersion(2, 3);
static const auto kMinVersionWithSafeMode = QnSoftwareVersion(2, 4, 1);
static const auto kMinVersionWithLocalId = QnSoftwareVersion(3, 0);

QnUuid getFactorySystemIdImpl(const QnUuid& serverId)
{
    return serverId;
}

QString generateTargetId(const QString& cloudSystemId,
    const QnUuid& localSystemId)
{
    return (cloudSystemId.isEmpty() ? localSystemId.toString() : cloudSystemId);
}

QnUuid getLocalSystemIdImpl(
    const QString& systemName,
    const QnUuid& localSystemId,
    const QnUuid& serverId,
    const QnSoftwareVersion& serverVersion)
{
    if (serverVersion < kMinVersionWithSystem)
        return serverId;    //< We have only one hub-server if version is less than 2.3

    if (serverVersion < kMinVersionWithLocalId)
    {
        if (systemName.isEmpty())
            return serverId; // Sometimes it happens and causes empty tile.

        return guidFromArbitraryData(systemName); //< No cloud, no local id, no new systems
    }

    if (localSystemId.isNull())
        return getFactorySystemIdImpl(serverId);  //< New System id

    return localSystemId;
}

QString getTargetSystemIdImpl(
    const QString& systemName,
    const QString& cloudSystemId,
    const QnUuid& localSystemId,
    const QnUuid& serverId,
    const QnSoftwareVersion& serverVersion)
{
    const auto fixedLocalId = getLocalSystemIdImpl(
        systemName, localSystemId, serverId, serverVersion);
    return generateTargetId(cloudSystemId, fixedLocalId);
}

} // namespace

namespace helpers {

QString getTargetSystemId(const QnConnectionInfo& info)
{
    return ::getTargetSystemIdImpl(info.systemName, info.cloudSystemId,
        info.localSystemId, info.serverId(), info.version);
}

QString getTargetSystemId(const QnModuleInformation& info)
{
    return ::getTargetSystemIdImpl(info.systemName, info.cloudSystemId,
        info.localSystemId, info.id, info.version);
}

QString getTargetSystemId(const QnCloudSystem& cloudSystem)
{
    return generateTargetId(cloudSystem.cloudId, cloudSystem.localId);
}

QnUuid getLocalSystemId(const QnModuleInformation& info)
{
    return getLocalSystemIdImpl(info.systemName, info.localSystemId, info.id, info.version);
}

QnUuid getLocalSystemId(const QnConnectionInfo& info)
{
    return getLocalSystemIdImpl(info.systemName, info.localSystemId, info.serverId(), info.version);
}

QString getFactorySystemId(const QnModuleInformation& info)
{
    return getFactorySystemIdImpl(info.id).toString();
}

bool isSafeMode(const QnModuleInformation& info)
{
    return (info.version < kMinVersionWithSafeMode ? false : info.ecDbReadOnly);
}

bool isNewSystem(const QnConnectionInfo& info)
{
    return info.localSystemId.isNull();
}

bool isNewSystem(const QnModuleInformation& info)
{
    if (info.version < kMinVersionWithLocalId)
        return false; //< No new systems until 3.0

    return info.localSystemId.isNull();
}

bool isNewSystem(const QnCloudSystem& info)
{
    return info.localId.isNull();
}

bool isCloudSystem(const QnModuleInformation& info)
{
    return (info.version < kMinVersionWithLocalId ? false : !info.cloudSystemId.isEmpty());
}

QnUuid currentSystemLocalId(const QnCommonModule* commonModule)
{
    if (!commonModule)
        return QnUuid();

    const auto& settings = commonModule->globalSettings();
    const auto localId = settings->localSystemId();
    return (localId.isNull() ? commonModule->remoteGUID() : localId);
}

bool serverBelongsToCurrentSystem(const QnModuleInformation& info, const QnCommonModule* commonModule)
{
    return (getLocalSystemId(info) == currentSystemLocalId(commonModule));
}

bool serverBelongsToCurrentSystem(const QnMediaServerResourcePtr& server)
{
    return serverBelongsToCurrentSystem(server->getModuleInformation(), server->commonModule());
}

bool currentSystemIsNew(const QnCommonModule* commonModule)
{
    const auto& settings = commonModule->globalSettings();
    return settings->localSystemId().isNull();
}

bool isLocalUser(const QString& login)
{
    return !login.contains(L'@');
}

} // namespace helpers
