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


#include <utils/common/app_info.h>

namespace {
    static const int configureProgress = 25;
    static const int waitProgress = 50;
    static const int completeProgress = 100;

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
    m_currentTask(0),
    m_updateTool(0)
{
}

QnConnectToCurrentSystemTool::~QnConnectToCurrentSystemTool() {}

void QnConnectToCurrentSystemTool::start(const QSet<QnUuid> &targets, const QString &password) {
    if (targets.isEmpty()) {
        finish(NoError);
        return;
    }

    m_targets = targets;
    m_password = password;
    m_restartTargets.clear();
    m_updateTargets.clear();
    m_waitTargets.clear();

    foreach (const QnUuid &id, m_targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            m_targets.remove(id);
    }

    configureServer();
}

QSet<QnUuid> QnConnectToCurrentSystemTool::targets() const {
    return m_targets;
}

void QnConnectToCurrentSystemTool::cancel() {
    if (m_currentTask)
        m_currentTask->cancel();

    if (m_updateTool)
        m_updateTool->cancelUpdate();

    if (m_currentTask || m_updateTool)
        emit finished(Canceled);
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode) {
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

    foreach (const QnUuid &id, m_targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QUrl url = server->apiConnection()->url();
        m_oldUrls.insert(id, url);
        url.setScheme(lit("http")); // TODO: #dklychkov Fix a bug in QNetworkAccessManager and use https
        url.setUserName(lit("admin"));
        url.setPassword(m_password);
        server->apiConnection()->setUrl(url);
    }

    QnConfigurePeerTask *task = new QnConfigurePeerTask(this);
    m_currentTask = task;

    connect(task, &QnNetworkPeerTask::finished, this, &QnConnectToCurrentSystemTool::at_configureTask_finished);
    task->setPasswordHash(adminUser->getHash(), adminUser->getDigest());
    task->setSystemName(qnCommon->localSystemName());
    task->start(m_targets);
}

void QnConnectToCurrentSystemTool::waitPeers() {
    QnWaitCompatibleServersPeerTask *task = new QnWaitCompatibleServersPeerTask(this);
    m_currentTask = task;

    connect(task, &QnNetworkPeerTask::finished, this, &QnConnectToCurrentSystemTool::at_waitTask_finished);
    task->start(QSet<QnUuid>::fromList(m_waitTargets.values()));

    emit progressChanged(configureProgress);
}

void QnConnectToCurrentSystemTool::updatePeers() {
    if (m_updateTargets.isEmpty()) {
        finish(NoError);
        return;
    }

    emit stateChanged(tr("Updating server(s)"));
    emit progressChanged(waitProgress);
    at_updateTool_progressChanged(0);

    m_updateTool = new QnMediaServerUpdateTool(this);
    connect(m_updateTool, &QnMediaServerUpdateTool::updateFinished, this, &QnConnectToCurrentSystemTool::at_updateTool_finished);

    m_updateTool->setTargets(m_updateTargets);
    m_updateTool->startUpdate(QnSoftwareVersion(), true);
}

void QnConnectToCurrentSystemTool::revertApiUrls() {
    for (auto it = m_oldUrls.begin(); it != m_oldUrls.end(); ++it) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(it.key(), true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        server->apiConnection()->setUrl(it.value());
    }
    m_oldUrls.clear();
}

void QnConnectToCurrentSystemTool::at_configureTask_finished(int errorCode, const QSet<QnUuid> &failedPeers) {
    m_currentTask = 0;
    revertApiUrls();

    if (errorCode != 0) {
        if (errorCode == QnConfigurePeerTask::AuthentificationFailed)
            finish(AuthentificationFailed);
        else
            finish(ConfigurationFailed);
        return;
    }

    foreach (const QnUuid &id, m_targets - failedPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (!isCompatible(server->getVersion(), qnCommon->engineVersion())) {
            m_updateTargets.insert(server->getId());
        } else {
            QnUuid originalId = QnUuid(server->getProperty(lit("guid")));
            if (!originalId.isNull())
                m_waitTargets.insert(server->getId(), originalId);
        }
    }

    waitPeers();
}

void QnConnectToCurrentSystemTool::at_waitTask_finished(int errorCode) {
    m_currentTask = 0;

    if (errorCode != 0) {
        finish(ConfigurationFailed);
        return;
    }

    updatePeers();
}

void QnConnectToCurrentSystemTool::at_updateTool_finished(const QnUpdateResult &result) {
    m_updateTool = 0;

    if (result.result == QnUpdateResult::Successful) {
        emit progressChanged(completeProgress);
        finish(NoError);
    } else {
        finish(UpdateFailed);
    }
}

void QnConnectToCurrentSystemTool::at_updateTool_progressChanged(int progress) {
    emit progressChanged(waitProgress + progress * static_cast<int>(QnFullUpdateStage::Count) / 100 / 2);
}
