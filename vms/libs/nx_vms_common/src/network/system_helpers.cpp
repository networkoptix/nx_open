// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_helpers.h"

#include <QtCore/QCoreApplication>

#include <core/resource/media_server_resource.h>
#include <network/cloud_system_data.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/id.h>

namespace {

struct SystemHelpers
{
    Q_DECLARE_TR_FUNCTIONS(SystemHelpers)
};

QnUuid getFactorySystemIdImpl(const QnUuid& serverId)
{
    return serverId;
}

QString generateTargetId(const QString& cloudSystemId,
    const QnUuid& localSystemId)
{
    return (cloudSystemId.isEmpty() ? localSystemId.toSimpleString() : cloudSystemId);
}

QnUuid getLocalSystemIdImpl(const QnUuid& localSystemId, const QnUuid& serverId)
{
    if (localSystemId.isNull())
        return getFactorySystemIdImpl(serverId);  //< New System id

    return localSystemId;
}

QString getTargetSystemIdImpl(
    const QString& cloudSystemId,
    const QnUuid& localSystemId,
    const QnUuid& serverId)
{
    const auto fixedLocalId = getLocalSystemIdImpl(
        localSystemId, serverId);
    return generateTargetId(cloudSystemId, fixedLocalId);
}

} // namespace

namespace helpers {

QString getTargetSystemId(const nx::vms::api::ModuleInformation& info)
{
    return ::getTargetSystemIdImpl(info.cloudSystemId, info.localSystemId, info.id);
}

QString getTargetSystemId(const QnCloudSystem& cloudSystem)
{
    return generateTargetId(cloudSystem.cloudId, cloudSystem.localId);
}

QnUuid getLocalSystemId(const nx::vms::api::ModuleInformation& info)
{
    return getLocalSystemIdImpl(info.localSystemId, info.id);
}

bool isNewSystem(const nx::vms::api::ModuleInformation& info)
{
    return info.localSystemId.isNull();
}

bool isNewSystem(const QnCloudSystem& info)
{
    return info.localId.isNull();
}

bool isCloudSystem(const nx::vms::api::ModuleInformation& info)
{
    return !info.cloudSystemId.isEmpty();
}

QString getSystemName(const nx::vms::api::ModuleInformation& info)
{
    return helpers::isNewSystem(info)
        ? SystemHelpers::tr("New Site")
        : info.systemName;
}

QnUuid currentSystemLocalId(const nx::vms::common::SystemContext* context)
{
    if (!context)
        return QnUuid();

    return context->globalSettings()->localSystemId();
}

bool serverBelongsToCurrentSystem(
    const nx::vms::api::ModuleInformation& info, const nx::vms::common::SystemContext* context)
{
    return !isNewSystem(info)
        && (getLocalSystemId(info) == currentSystemLocalId(context));
}

bool serverBelongsToCurrentSystem(const QnMediaServerResourcePtr& server)
{
    return serverBelongsToCurrentSystem(server->getModuleInformation(), server->systemContext());
}

bool currentSystemIsNew(const nx::vms::common::SystemContext* context)
{
    const auto& settings = context->globalSettings();
    return settings->localSystemId().isNull();
}

QString serverCloudHost(const QString& systemId, const QnUuid& serverId)
{
    return QString("%1.%2").arg(serverId.toSimpleString(), systemId);
}

} // namespace helpers
