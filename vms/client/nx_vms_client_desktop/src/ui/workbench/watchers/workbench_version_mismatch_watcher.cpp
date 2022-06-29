// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_version_mismatch_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

using Data = QnWorkbenchVersionMismatchWatcher::Data;

QnWorkbenchVersionMismatchWatcher::QnWorkbenchVersionMismatchWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,  this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (!server)
                return;

            connect(server.get(), &QnMediaServerResource::versionChanged,
                this, &QnWorkbenchVersionMismatchWatcher::updateComponents);

            connect(server.get(), &QnResource::statusChanged, this,
                [this](const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
                {
                    NX_DEBUG(NX_SCOPE_TAG, "Server %1 status changed", resource->getName());
                    updateComponents();
                });

            updateComponents();
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved,  this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (!server)
                return;

            server->disconnect(this);
            updateComponents();
        });

    for (const auto& server: resourcePool()->servers())
    {
        connect(server.get(), &QnMediaServerResource::versionChanged,
            this, &QnWorkbenchVersionMismatchWatcher::updateComponents);
        connect(server.get(), &QnResource::statusChanged, this,
            [this](const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
            {
                NX_DEBUG(NX_SCOPE_TAG, "Server %1 status changed", resource->getName());
                updateComponents();
            });
    }

    updateComponents();
}

QnWorkbenchVersionMismatchWatcher::~QnWorkbenchVersionMismatchWatcher()
{
}

bool QnWorkbenchVersionMismatchWatcher::hasMismatches() const
{
    return m_hasMismatches;
}

QList<Data> QnWorkbenchVersionMismatchWatcher::components() const
{
    return m_components;
}

nx::utils::SoftwareVersion QnWorkbenchVersionMismatchWatcher::latestVersion(
    Component component) const
{
    nx::utils::SoftwareVersion result;
    for (const auto& data: m_components)
    {
        if (component != Component::any && component != data.component)
            continue;

        result = qMax(data.version, result);
    }
    return result;
}

bool QnWorkbenchVersionMismatchWatcher::versionMismatches(const nx::utils::SoftwareVersion& left,
    const nx::utils::SoftwareVersion& right, bool concernBuild)
{
    return (left.major() != right.major() || left.minor() != right.minor()
        || left.bugfix() != right.bugfix() || (concernBuild && (left.build() != right.build())));
}

void QnWorkbenchVersionMismatchWatcher::updateHasMismatches()
{
    m_hasMismatches = false;

    if (m_components.isEmpty())
        return;

    nx::utils::SoftwareVersion newestComponent;
    nx::utils::SoftwareVersion newestServer;

    for (const auto& data: m_components)
    {
        newestComponent = qMax(data.version, newestComponent);
        if (data.component != Component::server)
            continue;
        newestServer = qMax(data.version, newestServer);
    }

    if (versionMismatches(newestComponent, newestServer, /*concernBuild*/ false))
        newestServer = newestComponent;

    m_hasMismatches = std::any_of(m_components.cbegin(), m_components.cend(),
        [newestServer](const Data& data)
        {
            return data.component == Component::server
                && versionMismatches(data.version, newestServer, /*concernBuild*/ true);
        });
}

void QnWorkbenchVersionMismatchWatcher::updateComponents()
{
    m_components.clear();

    Data clientData(Component::client, appContext()->version());
    m_components.push_back(clientData);

    for (const auto& server: resourcePool()->servers())
    {
        if (!server->isOnline())
            continue;
        Data msData(Component::server, server->getVersion());
        msData.server = server;
        m_components.push_back(msData);
    }

    updateHasMismatches();
    emit mismatchDataChanged();
}
