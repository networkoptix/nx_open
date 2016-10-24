
#include "system_helpers.h"

#include <network/cloud_system_data.h>
#include <network/module_information.h>
#include <api/model/connection_info.h>
#include <utils/common/id.h>

namespace {

static const auto kMinVersionWithSystem = QnSoftwareVersion(2, 3);
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

QString helpers::getTargetSystemId(const QnConnectionInfo& info)
{
    return ::getTargetSystemIdImpl(info.systemName, info.cloudSystemId,
        info.localSystemId, info.serverId(), info.version);
}

QString helpers::getTargetSystemId(const QnModuleInformation& info)
{
    return ::getTargetSystemIdImpl(info.systemName, info.cloudSystemId,
        info.localSystemId, info.id, info.version);
}

QString helpers::getTargetSystemId(const QnCloudSystem& cloudSystem)
{
    return generateTargetId(cloudSystem.cloudId, cloudSystem.localId);
}

QnUuid helpers::getLocalSystemId(const QnModuleInformation& info)
{
    return getLocalSystemIdImpl(info.systemName, info.localSystemId, info.id, info.version);
}

QnUuid helpers::getLocalSystemId(const QnConnectionInfo& info)
{
    return getLocalSystemIdImpl(info.systemName, info.localSystemId, info.serverId(), info.version);
}


QString helpers::getFactorySystemId(const QnModuleInformation& info)
{
    return getFactorySystemIdImpl(info.id).toString();
}

bool helpers::isNewSystem(const QnModuleInformation& info)
{
    if (info.version < kMinVersionWithLocalId)
        return false; //< No new systems until 3.0

    return info.localSystemId.isNull();
}

bool helpers::isNewSystem(const QnCloudSystem& info)
{
    return info.localId.isNull();
}
