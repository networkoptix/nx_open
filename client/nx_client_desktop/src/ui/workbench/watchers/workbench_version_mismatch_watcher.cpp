#include "workbench_version_mismatch_watcher.h"

#include <api/app_server_connection.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_context.h>

#include <utils/common/app_info.h>

QnWorkbenchVersionMismatchWatcher::QnWorkbenchVersionMismatchWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(context(), &QnWorkbenchContext::userChanged,
        this, &QnWorkbenchVersionMismatchWatcher::updateMismatchData);

    connect(resourcePool(), &QnResourcePool::resourceAdded,  this,
        [this](const QnResourcePtr&resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (!server)
                return;
            connect(server, &QnMediaServerResource::versionChanged,
                this, &QnWorkbenchVersionMismatchWatcher::updateMismatchData);
            updateMismatchData();
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved,  this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (!server)
                return;
            server->disconnect(this);
            updateMismatchData();
        });

    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        connect(server, &QnMediaServerResource::versionChanged,
            this, &QnWorkbenchVersionMismatchWatcher::updateMismatchData);
    }

    updateMismatchData();
}

QnWorkbenchVersionMismatchWatcher::~QnWorkbenchVersionMismatchWatcher()
{
}

bool QnWorkbenchVersionMismatchWatcher::hasMismatches() const
{
    return m_hasMismatches;
}

QList<QnAppInfoMismatchData> QnWorkbenchVersionMismatchWatcher::mismatchData() const
{
    return m_mismatchData;
}

nx::utils::SoftwareVersion QnWorkbenchVersionMismatchWatcher::latestVersion(
    Qn::SystemComponent component) const
{
    nx::utils::SoftwareVersion result;
    for (const auto& data: m_mismatchData)
    {
        if (component != Qn::AnyComponent && component != data.component)
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

    if (m_mismatchData.isEmpty())
        return;

    nx::utils::SoftwareVersion latest;
    nx::utils::SoftwareVersion latestMs;

    for (const auto& data: m_mismatchData)
    {
        latest = qMax(data.version, latest);
        if (data.component != Qn::ServerComponent)
            continue;
        latestMs = qMax(data.version, latestMs);
    }

    if (versionMismatches(latest, latestMs))
        latestMs = latest;

    for (const auto& data: m_mismatchData)
    {
        switch (data.component)
        {
            case Qn::ServerComponent:
                m_hasMismatches |= QnWorkbenchVersionMismatchWatcher::versionMismatches(
                    data.version, latestMs, true);
                break;
            default:
                break;
        }

        if (m_hasMismatches)
            break;
    }

}

void QnWorkbenchVersionMismatchWatcher::updateMismatchData()
{
    m_mismatchData.clear();

    QnAppInfoMismatchData clientData(Qn::ClientComponent, qnStaticCommon->engineVersion());
    m_mismatchData.push_back(clientData);

    if (context()->user())
    {
        for(const QnMediaServerResourcePtr& mediaServerResource: resourcePool()->getAllServers(Qn::AnyStatus))
        {
            QnAppInfoMismatchData msData(Qn::ServerComponent, mediaServerResource->getVersion());
            msData.resource = mediaServerResource;
            m_mismatchData.push_back(msData);
        }
    }

    updateHasMismatches();
    emit mismatchDataChanged();
}
