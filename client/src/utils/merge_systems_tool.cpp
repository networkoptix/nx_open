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
#include "utils/common/log.h"
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
    if (!m_serverByRequestHandle.isEmpty())
        return;

    m_foundModule.first = NotFoundError;

    foreach (const QnMediaServerResourcePtr &server, qnResPool->getAllServers()) {
        if (server->getStatus() != Qn::Online)
            continue;

        int handle = server->apiConnection()->pingSystemAsync(url, user, password, this, SLOT(at_pingSystem_finished(int,QnModuleInformation,int,QString)));
        m_serverByRequestHandle[handle] = server;
        NX_LOG(lit("QnMergeSystemsTool: ping request to %1 via %2").arg(url.toString()).arg(server->getApiUrl()), cl_logDEBUG1);
    }
}

int QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &user, const QString &password, bool ownSettings) {
    QString currentPassword = QnAppServerConnectionFactory::getConnection2()->authInfo();
    Q_ASSERT_X(!currentPassword.isEmpty(), "currentPassword cannot be empty", Q_FUNC_INFO);
    if (!ownSettings) {
        context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(false);
        m_password = password;
    }
    NX_LOG(lit("QnMergeSystemsTool: merge request to %1 url=%2").arg(proxy->getApiUrl()).arg(url.toString()), cl_logDEBUG1);
    return proxy->apiConnection()->mergeSystemAsync(url, user, password, currentPassword, ownSettings, false, false, this, SLOT(at_mergeSystem_finished(int,QnModuleInformation,int,QString)));
}

int QnMergeSystemsTool::configureIncompatibleServer(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &user, const QString &password) {
    QString currentPassword = QnAppServerConnectionFactory::getConnection2()->authInfo();
    Q_ASSERT_X(!currentPassword.isEmpty(), "currentPassword cannot be empty", Q_FUNC_INFO);
    return proxy->apiConnection()->mergeSystemAsync(url, user, password, currentPassword, true, true, true, this, SLOT(at_mergeSystem_finished(int,QnModuleInformation,int,QString)));
}

void QnMergeSystemsTool::at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString) {
    QnMediaServerResourcePtr server = m_serverByRequestHandle.take(handle);
    if (!server)
        return;

    NX_LOG(lit("QnMergeSystemsTool: ping response from %1 [%2 %3]").arg(server->getApiUrl()).arg(status).arg(errorString), cl_logDEBUG1);

    ErrorCode errorCode = (status == 0) ? errorStringToErrorCode(errorString) : InternalError;

    if (errorCode == NoError) {
        m_serverByRequestHandle.clear();
        emit systemFound(moduleInformation, server, NoError);
        return;
    }

    if (errorCode != NotFoundError) {
        m_foundModule.first = errorCode;
        m_foundModule.second = moduleInformation;
    }

    if (m_serverByRequestHandle.isEmpty())
        emit systemFound(m_foundModule.second, QnMediaServerResourcePtr(), m_foundModule.first);
}

void QnMergeSystemsTool::at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString) {
    NX_LOG(lit("QnMergeSystemsTool: merge reply id=%1 error=%2").arg(moduleInformation.id.toString()).arg(errorString), cl_logDEBUG1);

    if (status == 0 && errorString.isEmpty() && !m_password.isEmpty()) {
        context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(m_password);
        QUrl url = QnAppServerConnectionFactory::url();
        url.setPassword(m_password);
        QnAppServerConnectionFactory::setUrl(url);
    }
    m_password.clear();
    context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);

    if (status != 0)
        emit mergeFinished(InternalError, moduleInformation, handle);
    else
        emit mergeFinished(errorStringToErrorCode(errorString), moduleInformation, handle);
}
