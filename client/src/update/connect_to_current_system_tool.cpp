#include "connect_to_current_system_tool.h"

#include <QtWidgets/QMessageBox>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

#include <update/task/configure_peer_task.h>
#include <update/task/wait_compatible_servers_peer_task.h>
#include <update/media_server_update_tool.h>

#include <utils/network/global_module_finder.h>
#include <utils/common/software_version.h>


#include "version.h"

namespace {
    static const int configureProgress = 25;
    static const int waitProgress = 50;

    enum UpdateToolState {
        CheckingForUpdates,
        Updating
    };

    QnUserResourcePtr getAdminUser() {
        foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::user)) {
            QnUserResourcePtr user = resource.staticCast<QnUserResource>();
            if (user->getName() == lit("admin"))
                return user;
        }
        return QnUserResourcePtr();
    }
}

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QnWorkbenchContext *context, QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_running(false),
    m_configureTask(new QnConfigurePeerTask(this)),
    m_waitTask(new QnWaitCompatibleServersPeerTask(this)),
    m_updateTool(new QnMediaServerUpdateTool(this))
{
    connect(m_configureTask,            &QnNetworkPeerTask::finished,               this,       &QnConnectToCurrentSystemTool::at_configureTask_finished);
    connect(m_waitTask,                 &QnNetworkPeerTask::finished,               this,       &QnConnectToCurrentSystemTool::at_waitTask_finished);
    connect(m_updateTool,               &QnMediaServerUpdateTool::updateFinished,   this,       &QnConnectToCurrentSystemTool::at_updateTool_finished);
}

QnConnectToCurrentSystemTool::~QnConnectToCurrentSystemTool() {}

void QnConnectToCurrentSystemTool::connectToCurrentSystem(const QSet<QUuid> &targets, const QString &password) {
    if (m_running)
        return;

    if (targets.isEmpty()) {
        finish(NoError);
        return;
    }

    m_running = true;
    m_targets = targets;
    m_password = password;
    m_restartTargets.clear();
    m_updateTargets.clear();
    m_waitTargets.clear();

    foreach (const QUuid &id, m_targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            m_targets.remove(id);
    }

    configureServer();
}

bool QnConnectToCurrentSystemTool::isRunning() const {
    return m_running;
}

QSet<QUuid> QnConnectToCurrentSystemTool::targets() const {
    return m_targets;
}

void QnConnectToCurrentSystemTool::cancel() {
    if (!m_running)
        return;

    m_configureTask->cancel();
    m_waitTask->cancel();
    m_updateTool->cancelUpdate();

    emit canceled();
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode) {
    m_running = false;
    revertApiUrls();
    emit finished(errorCode);
}

void QnConnectToCurrentSystemTool::configureServer() {
    emit progressChanged(0);
    emit stateChanged(tr("Configuring server(s)"));

    QnUserResourcePtr adminUser = getAdminUser();
    if (!adminUser) {
        finish(ConfigurationFailed);
        return;
    }

    foreach (const QUuid &id, m_targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QUrl url = server->apiConnection()->url();
        m_oldUrls.insert(id, url);
        url.setScheme(lit("http")); // TODO: #dklychkov Fix a bug in QNetworkAccessManager and use https
        url.setUserName(lit("admin"));
        url.setPassword(m_password);
        server->apiConnection()->setUrl(url);
    }

    m_configureTask->setPasswordHash(adminUser->getHash(), adminUser->getDigest());
    m_configureTask->setSystemName(qnCommon->localSystemName());
    m_configureTask->start(m_targets);
}

void QnConnectToCurrentSystemTool::waitPeers() {
    m_waitTask->start(QSet<QUuid>::fromList(m_waitTargets.values()));
    emit progressChanged(configureProgress);
}

void QnConnectToCurrentSystemTool::updatePeers() {
    if (m_updateTargets.isEmpty()) {
        finish(NoError);
        return;
    }

    emit stateChanged(tr("Updating server(s)"));
    at_updateTool_progressChanged(0);

    m_updateFailed = false;
    m_updateTool->setTargets(m_updateTargets);
    m_updateTool->startUpdate(QnSoftwareVersion(), true);
}

void QnConnectToCurrentSystemTool::revertApiUrls() {
    for (auto it = m_oldUrls.begin(); it != m_oldUrls.end(); ++it) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(it.key()).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        server->apiConnection()->setUrl(it.value());
    }
    m_oldUrls.clear();
}

void QnConnectToCurrentSystemTool::at_configureTask_finished(int errorCode, const QSet<QUuid> &failedPeers) {
    if (!m_running)
        return;

    revertApiUrls();

    if (errorCode != 0) {
        if (errorCode == QnConfigurePeerTask::AuthentificationFailed)
            finish(AuthentificationFailed);
        else
            finish(ConfigurationFailed);
        return;
    }

    foreach (const QUuid &id, m_targets - failedPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (!isCompatible(server->getVersion(), qnCommon->engineVersion()))
            m_updateTargets.insert(server->getId());
        else
            m_waitTargets.insert(server->getId(), QUuid(server->getProperty(lit("guid"))));
    }

    waitPeers();
}

void QnConnectToCurrentSystemTool::at_waitTask_finished(int errorCode) {
    if (!m_running)
        return;

    if (errorCode != 0) {
        finish(ConfigurationFailed);
        return;
    }

    updatePeers();
}

void QnConnectToCurrentSystemTool::at_updateTool_finished(const QnUpdateResult &result) {
    if (!m_running)
        return;

    finish(result.result == QnUpdateResult::Successful ? NoError : UpdateFailed);
}

void QnConnectToCurrentSystemTool::at_updateTool_progressChanged(int progress) {
    emit progressChanged(waitProgress + progress * static_cast<int>(QnFullUpdateStage::Count) / 100 / 2);
}
