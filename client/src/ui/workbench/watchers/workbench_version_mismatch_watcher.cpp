#include "workbench_version_mismatch_watcher.h"

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_context.h>

#include <version.h>

QnWorkbenchVersionMismatchWatcher::QnWorkbenchVersionMismatchWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(context(),  SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(updateMismatchData()));
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

    QnVersionMismatchData first = m_mismatchData[0];
    foreach(const QnVersionMismatchData &second, m_mismatchData) {
        if(versionMismatches(first.version,
                             second.version,
                             first.component == Qn::MediaServerComponent && second.component == Qn::MediaServerComponent)) {
            m_hasMismatches = true;
            break;
        }
    }
}

void QnWorkbenchVersionMismatchWatcher::updateMismatchData() {
    m_mismatchData.clear();

    QnVersionMismatchData clientData(Qn::ClientComponent, QnSoftwareVersion(QN_ENGINE_VERSION));
    m_mismatchData.push_back(clientData);

    if(context()->user()) {
        QnVersionMismatchData ecData(Qn::EnterpriseControllerComponent, QnAppServerConnectionFactory::currentVersion());
        m_mismatchData.push_back(ecData);

        foreach(const QnMediaServerResourcePtr &mediaServerResource, resourcePool()->getResources().filtered<QnMediaServerResource>()) {
            QnVersionMismatchData msData(Qn::MediaServerComponent, mediaServerResource->getVersion());
            msData.resource = mediaServerResource;
            m_mismatchData.push_back(msData);
        }
    }

    updateHasMismatches();
    emit mismatchDataChanged();
}
