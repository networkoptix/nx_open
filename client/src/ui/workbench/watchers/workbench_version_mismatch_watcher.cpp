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

QnSoftwareVersion QnWorkbenchVersionMismatchWatcher::latestVersion() const {
    QnSoftwareVersion result;
    foreach(const QnVersionMismatchData &data, m_mismatchData)
        result = qMax(data.version, result);
    return result;
}

void QnWorkbenchVersionMismatchWatcher::updateHasMismatches() {
    m_hasMismatches = false;
    
    if(m_mismatchData.isEmpty())
        return;

    QnSoftwareVersion version = m_mismatchData[0].version;
    foreach(const QnVersionMismatchData &data, m_mismatchData) {
        if(!isCompatible(data.version, version)) {
            m_hasMismatches = true;
            break;
        }
    }
}

void QnWorkbenchVersionMismatchWatcher::updateMismatchData() {
    m_mismatchData.clear();

    QnVersionMismatchData clientData;
    clientData.component = Qn::ClientComponent;
    clientData.version = QnSoftwareVersion(QN_ENGINE_VERSION);
    m_mismatchData.push_back(clientData);

    if(context()->user()) {
        QnVersionMismatchData ecData;
        ecData.component = Qn::EnterpriseControllerComponent;
        ecData.version = QnAppServerConnectionFactory::currentVersion();
        m_mismatchData.push_back(ecData);

        foreach(const QnMediaServerResourcePtr &mediaServerResource, resourcePool()->getResources().filtered<QnMediaServerResource>()) {
            QnVersionMismatchData msData;
            msData.component = Qn::MediaServerComponent;
            msData.resource = mediaServerResource;
            msData.version = mediaServerResource->getVersion();
            m_mismatchData.push_back(msData);
        }
    }

    updateHasMismatches();
    emit mismatchDataChanged();
}
