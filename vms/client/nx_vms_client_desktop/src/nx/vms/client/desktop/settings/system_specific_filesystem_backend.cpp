// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_specific_filesystem_backend.h"

#include <QtCore/QStandardPaths>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

QDir calculatePath(SystemContext* systemContext)
{
    static const QString kLocalSettingsPlaceholder = "local";

    const auto connection = systemContext->connection();
    const QString systemId = connection
        ? connection->moduleInformation().localSystemId.toString()
        : kLocalSettingsPlaceholder;

    return QDir(QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
        + "/settings/"
        + systemId);
}

} // namespace

using namespace nx::utils::property_storage;

SystemSpecificFileSystemBackend::SystemSpecificFileSystemBackend(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    FileSystemBackend(calculatePath(systemContext)),
    SystemContextAware(systemContext)
{
    connect(systemContext,
        &SystemContext::remoteIdChanged,
        this,
        [this]
        {
            m_path = calculatePath(this->systemContext());
            NX_DEBUG(this, "Local settings backend changed to %1", m_path.absolutePath());
        });
}

} // namespace nx::vms::client::desktop
