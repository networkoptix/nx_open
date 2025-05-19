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
    static const QString kLocalSettingsPlaceholder = "_logged_out_";

    const auto systemId = systemContext->moduleInformation().localSystemId;
    const QString systemIdPath = systemId.isNull()
        ? kLocalSettingsPlaceholder
        : systemId.toString();

    return QDir(QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
        + "/settings/"
        + systemIdPath);
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
        [this](Uuid serverId)
        {
            m_valid = !serverId.isNull();
            NX_ASSERT(!m_valid || this->systemContext()->connection(),
                "Connection must exist for the settings to be loaded");
            m_path = calculatePath(this->systemContext());
            NX_DEBUG(this, "Local settings backend changed to %1", m_path.absolutePath());
        });
}

bool SystemSpecificFileSystemBackend::SystemSpecificFileSystemBackend::isWritable() const
{
    return m_valid && base_type::isWritable();
}

QString SystemSpecificFileSystemBackend::readValue(const QString& name, bool* success)
{
    if (!NX_ASSERT(m_valid))
        return QString();

    return base_type::readValue(name, success);
}

bool SystemSpecificFileSystemBackend::writeValue(const QString& name, const QString& value)
{
    if (!NX_ASSERT(m_valid))
        return false;

    return base_type::writeValue(name, value);
}

bool SystemSpecificFileSystemBackend::removeValue(const QString& name)
{
    if (!NX_ASSERT(m_valid))
        return false;

    return base_type::removeValue(name);
}

bool SystemSpecificFileSystemBackend::exists(const QString& name) const
{
    if (!NX_ASSERT(m_valid))
        return false;

    return base_type::exists(name);
}

bool SystemSpecificFileSystemBackend::sync()
{
    if (!NX_ASSERT(m_valid))
        return false;

    return base_type::sync();
}

bool SystemSpecificFileSystemBackend::writeDocumentation(const QString& docText)
{
    if (!NX_ASSERT(m_valid))
        return false;

    return base_type::writeDocumentation(docText);
}

} // namespace nx::vms::client::desktop
