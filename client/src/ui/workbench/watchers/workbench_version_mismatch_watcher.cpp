#include "workbench_version_mismatch_watcher.h"

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <client_message_processor.h>
#include <version.h>

QnWorkbenchVersionMismatchWatcher::QnWorkbenchVersionMismatchWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(QnClientMessageProcessor::instance(),               SIGNAL(connectionClosed()),                     this,   SLOT(at_messageProcessor_connectionClosed()));
    connect(QnClientMessageProcessor::instance(),               SIGNAL(connectionOpened()),                     this,   SLOT(at_messageProcessor_connectionOpened()));

    at_messageProcessor_connectionClosed();
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

void QnWorkbenchVersionMismatchWatcher::at_messageProcessor_connectionClosed() {
    m_mismatchData.clear();

    QnVersionMismatchData clientData;
    clientData.component = Qn::ClientComponent;
    clientData.version = QnSoftwareVersion(QN_ENGINE_VERSION);
    m_mismatchData.push_back(clientData);

    updateHasMismatches();
    emit mismatchDataChanged();
}

void QnWorkbenchVersionMismatchWatcher::at_messageProcessor_connectionOpened() {
    m_mismatchData.clear();
    
    QnVersionMismatchData clientData;
    clientData.component = Qn::ClientComponent;
    clientData.version = QnSoftwareVersion(QN_ENGINE_VERSION);
    m_mismatchData.push_back(clientData);

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

    updateHasMismatches();
    emit mismatchDataChanged();
}
