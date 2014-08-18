#include "connect_to_current_system_tool.h"

#include <QtWidgets/QMessageBox>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <common/common_module.h>
#include <utils/network/global_module_finder.h>
#include <utils/configure_peer_task.h>
#include <utils/media_server_update_tool.h>
#include <utils/common/software_version.h>
#include <ui/dialogs/update_dialog.h>

#include "version.h"

namespace {
    enum UpdateToolState {
        CheckingForUpdates,
        Updating
    };
}

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QnWorkbenchContext *context, QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(context),
    m_running(false),
    m_configureTask(new QnConfigurePeerTask(this)),
    m_updateDialog(new QnUpdateDialog(context))
{
    m_updateTool = m_updateDialog->updateTool();

    connect(m_configureTask,            &QnNetworkPeerTask::finished,               this,       &QnConnectToCurrentSystemTool::at_configureTask_finished);
    // queued connection is used to be sure that we'll get this signal AFTER it will be handled by update dialog
    connect(m_updateTool,               &QnMediaServerUpdateTool::stateChanged,     this,       &QnConnectToCurrentSystemTool::at_updateTool_stateChanged, Qt::QueuedConnection);
}

QnConnectToCurrentSystemTool::~QnConnectToCurrentSystemTool() {}

void QnConnectToCurrentSystemTool::connectToCurrentSystem(const QSet<QUuid> &targets, const QString &password) {
    if (m_running)
        return;

    m_running = true;
    m_targets = targets;
    m_password = password;
    m_restartTargets.clear();
    m_updateTargets.clear();
    configureServer();
}

bool QnConnectToCurrentSystemTool::isRunning() const {
    return m_running;
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode) {
    m_running = false;
    m_updateDialog->hide();
    revertApiUrls();
    emit finished(errorCode);
}

void QnConnectToCurrentSystemTool::configureServer() {
    if (m_targets.isEmpty()) {
        finish(NoError);
        return;
    }

    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::user)) {
        QnUserResourcePtr user = resource.staticCast<QnUserResource>();
        if (user->getName() == lit("admin")) {
            m_configureTask->setPasswordHash(user->getHash(), user->getDigest());
            break;
        }
    }

    foreach (const QUuid &id, m_targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QUrl url = server->apiConnection()->url();
        m_oldUrls.insert(id, url);
        url.setScheme(lit("https"));
        url.setUserName(lit("admin"));
        url.setPassword(m_password);
        server->apiConnection()->setUrl(url);
    }

    m_configureTask->setSystemName(qnCommon->localSystemName());
    m_configureTask->start(m_targets);
}

void QnConnectToCurrentSystemTool::updatePeers() {
    if (m_updateTargets.isEmpty()) {
        finish(NoError);
        return;
    }

    m_updateFailed = false;
    m_updateDialog->setTargets(m_updateTargets);
    m_updateDialog->show();
    m_prevToolState = CheckingForUpdates;
    m_updateTool->checkForUpdates(qnCommon->engineVersion());
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
    revertApiUrls();

    if (errorCode != 0) {
        if (errorCode == QnConfigurePeerTask::AuthentificationFailed)
            finish(AuthentificationFailed);
        else
            finish(ConfigurationFailed);
        return;
    }

    foreach (const QUuid &id, m_targets - failedPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (server->getVersion() != qnCommon->engineVersion())
            m_updateTargets.insert(id);
    }

    updatePeers();
}

void QnConnectToCurrentSystemTool::at_updateTool_stateChanged(int state) {
    if (state != QnMediaServerUpdateTool::Idle)
        return;

    if (m_prevToolState == CheckingForUpdates) {
        switch (m_updateTool->updateCheckResult()) {
        case QnMediaServerUpdateTool::UpdateFound:
            m_prevToolState = Updating;
            m_updateTool->updateServers();
            return;
        case QnMediaServerUpdateTool::NoNewerVersion:
            break;
        default:
            m_updateFailed = true;
            break;
        }
    } else if (m_prevToolState == Updating) {
        m_updateFailed = (m_updateTool->updateResult() != QnMediaServerUpdateTool::UpdateSuccessful);
    }

    m_updateDialog->hide();
    finish(m_updateFailed ? UpdateFailed : NoError);
}
