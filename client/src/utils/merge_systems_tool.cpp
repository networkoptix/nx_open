#include "merge_systems_tool.h"

#include <QtCore/QTimer>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkReply>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"
#include "common/common_module.h"
#include "utils/network/module_finder.h"
#include "client/client_settings.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/watchers/workbench_user_watcher.h"
#include "ui/actions/action_manager.h"
#include "ui/actions/action.h"

namespace {

    QnMergeSystemsTool::ErrorCode errorStringToErrorCode(const QString &str) {
        if (str.isEmpty())
            return QnMergeSystemsTool::NoError;
        if (str == lit("FAIL"))
            return QnMergeSystemsTool::NotFoundError;
        else if (str == lit("INCOMPATIBLE"))
            return QnMergeSystemsTool::VersionError;
        else if (str == lit("UNAUTHORIZED"))
            return QnMergeSystemsTool::AuthentificationError;
        else if (str == lit("BACKUP_ERROR"))
            return QnMergeSystemsTool::BackupError;
        else
            return QnMergeSystemsTool::InternalError;
    }

} // anonymous namespace

QnMergeSystemsTool::QnMergeSystemsTool(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

void QnMergeSystemsTool::pingSystem(const QUrl &url, const QString &user, const QString &password) {
    m_serverByRequestHandle.clear();
    foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllServers()) {
        if (server->getStatus() != Qn::Online)
            continue;

        int handle = server->apiConnection()->pingSystemAsync(url, user, password, this, SLOT(at_pingSystem_finished(int,QnModuleInformation,int,QString)));
        m_serverByRequestHandle[handle] = server;
    }
}

int QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &user, const QString &password, bool ownSettings, bool oneServer) {
    QString currentPassword = QnAppServerConnectionFactory::getConnection2()->authInfo();
    Q_ASSERT_X(!currentPassword.isEmpty(), "currentPassword cannot be empty", Q_FUNC_INFO);
    if (!ownSettings) {
        context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(false);
        m_password = password;
    }
    return proxy->apiConnection()->mergeSystemAsync(url, user, password, currentPassword, ownSettings, oneServer, this, SLOT(at_mergeSystem_finished(int,QnModuleInformation,int,QString)));
}

void QnMergeSystemsTool::at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString) {
    QnMediaServerResourcePtr server = m_serverByRequestHandle.take(handle);
    if (!server)
        return;

    ErrorCode errorCode = errorStringToErrorCode(errorString);

    if (!errorString.isEmpty() || status != 0) {
        if (errorCode == NotFoundError) {
            if (m_serverByRequestHandle.isEmpty())
                emit systemFound(QnModuleInformation(), QnMediaServerResourcePtr(), NotFoundError);
            return;
        }

        m_serverByRequestHandle.clear();

        emit systemFound(moduleInformation, QnMediaServerResourcePtr(), errorCode);
        return;
    }

    m_serverByRequestHandle.clear();
    emit systemFound(moduleInformation, server, NoError);
}

void QnMergeSystemsTool::at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString) {
    bool reconnect = false;
    if (status == 0 && errorString.isEmpty() && !m_password.isEmpty()) {
        context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(m_password);
        QUrl url = QnAppServerConnectionFactory::url();
        url.setPassword(m_password);
        QnAppServerConnectionFactory::setUrl(url);
        reconnect = true;
    }
    m_password.clear();
    context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);

    if (status != 0)
        emit mergeFinished(InternalError, moduleInformation, handle);
    else
        emit mergeFinished(errorStringToErrorCode(errorString), moduleInformation, handle);

    if (reconnect)
        menu()->action(Qn::ReconnectAction)->trigger();
}
