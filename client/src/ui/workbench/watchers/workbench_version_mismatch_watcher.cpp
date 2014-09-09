#include "workbench_version_mismatch_watcher.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

#include <ui/workbench/workbench_context.h>

#include <version.h>

QnWorkbenchVersionMismatchWatcher::QnWorkbenchVersionMismatchWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(context(),  SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(updateMismatchData()));

    connect(qnResPool, &QnResourcePool::resourceAdded,  this, [this] (const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;
        connect(server, &QnMediaServerResource::versionChanged, this, &QnWorkbenchVersionMismatchWatcher::updateMismatchData);
        updateMismatchData();
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved,  this, [this] (const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;
        disconnect(server, NULL, this, NULL);
        updateMismatchData();
    });

    updateMismatchData();
}

QnWorkbenchVersionMismatchWatcher::~QnWorkbenchVersionMismatchWatcher() {
    return;
}

bool QnWorkbenchVersionMismatchWatcher::hasMismatches() const {
    return m_hasMismatches;
}

QList<QnVersionMismatchData> QnWorkbenchVersionMismatchWatcher::mismatchData() const {
    return m_mismatchData;
}

QnSoftwareVersion QnWorkbenchVersionMismatchWatcher::latestVersion(Qn::SystemComponent component) const {
    QnSoftwareVersion result;
    foreach(const QnVersionMismatchData &data, m_mismatchData) {
        if (component != Qn::AnyComponent && component != data.component)
            continue;
        result = qMax(data.version, result);
    }
    return result;
}

bool QnWorkbenchVersionMismatchWatcher::versionMismatches(QnSoftwareVersion left, QnSoftwareVersion right, bool concernBuild) {
    return (left.major() != right.major() ||
            left.minor() != right.minor() ||
            left.bugfix() != right.bugfix() ||
            (concernBuild &&
             (left.build() != right.build())
             )
            );
}

void QnWorkbenchVersionMismatchWatcher::updateHasMismatches() {
    m_hasMismatches = false;
    
    if(m_mismatchData.isEmpty())
        return;

    QnSoftwareVersion latest;
    QnSoftwareVersion latestMs;
    foreach(const QnVersionMismatchData &data, m_mismatchData) {
        latest = qMax(data.version, latest);
        if (data.component != Qn::ServerComponent)
            continue;
        latestMs = qMax(data.version, latestMs);
    }
    if (versionMismatches(latest, latestMs))
        latestMs = latest;

    foreach(const QnVersionMismatchData &data, m_mismatchData) {
        switch (data.component) {
        case Qn::ServerComponent:
            m_hasMismatches |= QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestMs, true);
            break;
        default:
            break;
        }
        if (m_hasMismatches)
            break;
    }

}

void QnWorkbenchVersionMismatchWatcher::updateMismatchData() {
    m_mismatchData.clear();

    QnVersionMismatchData clientData(Qn::ClientComponent, qnCommon->engineVersion());
    m_mismatchData.push_back(clientData);

    if(context()->user()) {
        foreach(const QnMediaServerResourcePtr &mediaServerResource, resourcePool()->getResources().filtered<QnMediaServerResource>()) {
            QnVersionMismatchData msData(Qn::ServerComponent, mediaServerResource->getVersion());
            msData.resource = mediaServerResource;
            m_mismatchData.push_back(msData);
        }
    }

    updateHasMismatches();
    emit mismatchDataChanged();
}
