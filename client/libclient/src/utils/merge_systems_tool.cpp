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
#include <network/authutil.h>

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
        else if (str == lit("NOT_LOCAL_OWNER"))
            return QnMergeSystemsTool::notLocalOwner;
        else if (str == lit("BACKUP_ERROR"))
            return QnMergeSystemsTool::BackupError;
        else if (str == lit("STARTER_LICENSE_ERROR"))
            return QnMergeSystemsTool::StarterLicenseError;
        else if (str == lit("SAFE_MODE"))
            return QnMergeSystemsTool::SafeModeError;
        else if (str == lit("CONFIGURATION_ERROR"))
            return QnMergeSystemsTool::ConfigurationError;
        else if (str == lit("DEPENDENT_SYSTEM_BOUND_TO_CLOUD"))
            return QnMergeSystemsTool::DependentSystemBoundToCloudError;
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

void QnMergeSystemsTool::pingSystem(const QUrl& url, const QAuthenticator& userAuth)
{
    if (!m_serverByRequestHandle.isEmpty())
        return;

    m_foundModule.first = NotFoundError;


    PingSystemCtx ctx;
    ctx.url = url;
    ctx.auth = userAuth;

    QnForeignServerConnectionPtr remoteConnection(new QnForeignServerConnection());
    remoteConnection->setUrl(url);
    ctx.remoteConnection = remoteConnection;
    int handle = remoteConnection->getNonceAsync(url, this, SLOT(at_getNonceForPingFinished(int, QnGetNonceReply, int, QString)));
    m_pingSystemRequests[handle] = ctx;
}


int QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QAuthenticator& userAuth, bool ownSettings)
{
    NX_LOG(lit("QnMergeSystemsTool: merge request to %1 url=%2").arg(proxy->getApiUrl().toString()).arg(url.toString()), cl_logDEBUG1);

    MergeSystemCtx ctx;
    ctx.proxy = proxy;
    ctx.url = url;
    ctx.auth = userAuth;
    ctx.ownSettings = ownSettings;

    QnForeignServerConnectionPtr remoteConnection(new QnForeignServerConnection());
    remoteConnection->setUrl(url);
    ctx.remoteConnection = remoteConnection;
    ctx.nonceRequestHandle = remoteConnection->getNonceAsync(url, this, SLOT(at_getNonceForMergeFinished(int, QnGetNonceReply, int, QString)));
    m_mergeSystemRequests[ctx.nonceRequestHandle] = ctx;
    return ctx.nonceRequestHandle;
}

void QnMergeSystemsTool::at_getNonceForMergeFinished(
    int status,
    const QnGetNonceReply& nonceReply,
    int handle,
    const QString& errorString)
{
    MergeSystemCtx& ctx = m_mergeSystemRequests[handle];
    ctx.remoteConnection.clear();

    QByteArray getKey = createHttpQueryAuthParam(
        ctx.auth.user(),
        ctx.auth.password(),
        nonceReply.realm,
        "GET",
        nonceReply.nonce.toUtf8());

    QByteArray postKey = createHttpQueryAuthParam(
        ctx.auth.user(),
        ctx.auth.password(),
        nonceReply.realm,
        "POST",
        nonceReply.nonce.toUtf8());

    ctx.mergeRequestHandle = ctx.proxy->apiConnection()->mergeSystemAsync(
        ctx.url,
        QString::fromLatin1(getKey),
        QString::fromLatin1(postKey),
        ctx.ownSettings,
        ctx.oneServer,
        ctx.ignoreIncompatible,
        this,
        SLOT(at_mergeSystem_finished(int, QnModuleInformation, int, QString)));
}

void QnMergeSystemsTool::at_getNonceForPingFinished(
    int status,
    const QnGetNonceReply& nonceReply,
    int handle,
    const QString& errorString)
{
    PingSystemCtx& ctx = m_pingSystemRequests[handle];

    QByteArray getKey = createHttpQueryAuthParam(
        ctx.auth.user(),
        ctx.auth.password(),
        nonceReply.realm,
        "GET",
        nonceReply.nonce.toUtf8());

    const auto onlineServers = qnResPool->getAllServers(Qn::Online);
    for (const QnMediaServerResourcePtr &server : onlineServers)
    {
        int handle = server->apiConnection()->pingSystemAsync(
            ctx.url,
            QLatin1String(getKey),
            this,
            SLOT(at_pingSystem_finished(int, QnModuleInformation, int, QString)));
        m_serverByRequestHandle[handle] = server;
        NX_LOG(lit("QnMergeSystemsTool: ping request to %1 via %2").arg(ctx.url.toString()).arg(server->getApiUrl().toString()), cl_logDEBUG1);
    }

    m_pingSystemRequests.remove(handle);
}

int QnMergeSystemsTool::configureIncompatibleServer(const QnMediaServerResourcePtr &proxy, const QUrl &url, const QAuthenticator& userAuth)
{
    MergeSystemCtx ctx;
    ctx.proxy = proxy;
    ctx.url = url;
    ctx.auth = userAuth;
    ctx.ownSettings = true;
    ctx.oneServer = true;
    ctx.ignoreIncompatible = true;

    int handle = proxy->apiConnection()->getNonceAsync(url, this, SLOT(at_getNonceForMergeFinished(int, QnGetNonceReply, int, QString)));
    m_mergeSystemRequests[handle] = ctx;
    return handle;
}

void QnMergeSystemsTool::at_pingSystem_finished(int status, const QnModuleInformation &moduleInformation, int handle, const QString &errorString)
{
    QnMediaServerResourcePtr server = m_serverByRequestHandle.take(handle);
    if (!server)
        return;

    NX_LOG(lit("QnMergeSystemsTool: ping response from %1 [%2 %3]")
        .arg(server->getApiUrl().toString()).arg(status).arg(errorString),
        cl_logDEBUG1);

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
    bool ctxFound = false;
    for (const auto& ctx : m_mergeSystemRequests)
    {
        if (ctx.mergeRequestHandle == handle)
        {
            handle = ctx.nonceRequestHandle;
            m_mergeSystemRequests.remove(handle);
            ctxFound = true;
            break;
        }
    }
    if (!ctxFound)
        return;

    NX_LOG(lit("QnMergeSystemsTool: merge reply id=%1 error=%2").arg(moduleInformation.id.toString()).arg(errorString), cl_logDEBUG1);

    QnMergeSystemsTool::ErrorCode errCode = InternalError;
    if (status == QNetworkReply::ContentOperationNotPermittedError)
        errCode = ForbiddenError;
    else if (status == 0)
        errCode = errorStringToErrorCode(errorString);

    emit mergeFinished(errCode, moduleInformation, handle);
}
