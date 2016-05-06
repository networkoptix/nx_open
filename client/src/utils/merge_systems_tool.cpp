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
#include "network/module_finder.h"
#include <nx/utils/log/log.h>
#include "client/client_settings.h"
#include "ui/workbench/workbench_context.h"
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
        else if (str == lit("STARTER_LICENSE_ERROR"))
            return QnMergeSystemsTool::StarterLicenseError;
        else if (str == lit("SAFE_MODE"))
            return QnMergeSystemsTool::SafeModeError;
        else if (str == lit("CONFIGURATION_ERROR"))
            return QnMergeSystemsTool::ConfigurationError;
        else if (str == lit("BOTH_SYSTEMS_BOUND_TO_CLOUD"))
            return QnMergeSystemsTool::BothSystemsBoundToCloudError;
        else if (str == lit("UNCONFIGURED_SYSTEM"))
            return QnMergeSystemsTool::UnconfiguredSystemError;
        else
            return QnMergeSystemsTool::InternalError;
    }

} // anonymous namespace

QnMergeSystemsTool::QnMergeSystemsTool(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

void QnMergeSystemsTool::pingSystem(const QUrl &url, const QString &password)
{
    if (!m_serverByRequestHandle.isEmpty())
        return;

    m_foundModule.first = NotFoundError;

    const auto onlineServers = qnResPool->getAllServers(Qn::Online);
    for (const QnMediaServerResourcePtr &server: onlineServers)
    {
        int handle = server->apiConnection()->pingSystemAsync(url, password, this, SLOT(at_pingSystem_finished(int,QnModuleInformation,int,QString)));
        m_serverByRequestHandle[handle] = server;
        NX_LOG(lit("QnMergeSystemsTool: ping request to %1 via %2").arg(url.toString()).arg(server->getApiUrl()), cl_logDEBUG1);
    }
}

int QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &password, bool ownSettings)
{
    QString currentPassword = QnAppServerConnectionFactory::getConnection2()->authInfo();
    NX_ASSERT(!currentPassword.isEmpty(), "currentPassword cannot be empty", Q_FUNC_INFO);
    NX_LOG(lit("QnMergeSystemsTool: merge request to %1 url=%2").arg(proxy->getApiUrl()).arg(url.toString()), cl_logDEBUG1);
    return proxy->apiConnection()->mergeSystemAsync(url, password, currentPassword, ownSettings, false, false, this, SLOT(at_mergeSystem_finished(int,QnModuleInformation,int,QString)));
}

int QnMergeSystemsTool::configureIncompatibleServer(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QString &password)
{
    QString currentPassword = QnAppServerConnectionFactory::getConnection2()->authInfo();
    NX_ASSERT(!currentPassword.isEmpty(), "currentPassword cannot be empty", Q_FUNC_INFO);
    return proxy->apiConnection()->mergeSystemAsync(url, password, currentPassword, true, true, true, this, SLOT(at_mergeSystem_finished(int,QnModuleInformation,int,QString)));
}

void QnMergeSystemsTool::at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString)
{
    QnMediaServerResourcePtr server = m_serverByRequestHandle.take(handle);
    if (!server)
        return;

    NX_LOG(lit("QnMergeSystemsTool: ping response from %1 [%2 %3]").arg(server->getApiUrl()).arg(status).arg(errorString), cl_logDEBUG1);

    ErrorCode errorCode = (status == 0) ? errorStringToErrorCode(errorString) : InternalError;

    auto isOk = [](ErrorCode errorCode) { return errorCode == NoError || errorCode == StarterLicenseError; };

    if (isOk(errorCode) && moduleInformation.ecDbReadOnly)
        errorCode = ErrorCode::SafeModeError;

    if (isOk(errorCode))
    {
        m_serverByRequestHandle.clear();
        emit systemFound(moduleInformation, server, errorCode);
        return;
    }

    if (errorCode != NotFoundError)
    {
        m_foundModule.first = errorCode;
        m_foundModule.second = moduleInformation;
    }

    if (m_serverByRequestHandle.isEmpty())
        emit systemFound(m_foundModule.second, QnMediaServerResourcePtr(), m_foundModule.first);
}

void QnMergeSystemsTool::at_mergeSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString)
{
    NX_LOG(lit("QnMergeSystemsTool: merge reply id=%1 error=%2").arg(moduleInformation.id.toString()).arg(errorString), cl_logDEBUG1);

    QnMergeSystemsTool::ErrorCode errCode = InternalError;
    if (status == QNetworkReply::ContentOperationNotPermittedError)
        errCode = ForbiddenError;
    else if (status == 0)
        errCode = errorStringToErrorCode(errorString);

    emit mergeFinished(errCode, moduleInformation, handle);
}
