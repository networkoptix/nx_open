#include "system_helpers.h"

#include <network/cloud_system_data.h>
#include <network/module_information.h>
#include <api/model/connection_info.h>
#include <utils/common/id.h>

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

bool isSafeModeImpl(
    const QnSoftwareVersion& version,
    bool ecDbReadOnly)
{
    return (version < kMinVersionWithSafeMode ? false : ecDbReadOnly);
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
    return isSafeModeImpl(info.version, info.ecDbReadOnly);
}

bool isNewSystem(const QnConnectionInfo& info)
{
    if (isSafeModeImpl(info.version, info.ecDbReadOnly))
        return false;

    return info.localSystemId.isNull();
}

bool isNewSystem(const QnModuleInformation& info)
{
    if (isSafeMode(info))
        return false;

    if (info.version < kMinVersionWithLocalId)
        return false; //< No new systems until 3.0

    return info.localSystemId.isNull();
}

bool isNewSystem(const QnCloudSystem& info)
{
    return info.localId.isNull();
}

} // namespace helpers
