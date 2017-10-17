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
#include "nx/vms/discovery/manager.h"
#include <nx/utils/log/log.h>
#include "client/client_settings.h"
#include <network/authutil.h>
#include <nx/network/http/asynchttpclient.h>

QnMergeSystemsTool::QnMergeSystemsTool(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

void QnMergeSystemsTool::pingSystem(const nx::utils::Url &url, const QAuthenticator& userAuth)
{
    if (!m_twoStepRequests.isEmpty())
        return;

    m_foundModule.first = utils::MergeSystemsStatus::notFound;

    TwoStepRequestCtx ctx;
    ctx.url = url;
    ctx.auth = userAuth;

    const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
    for (const QnMediaServerResourcePtr &server: onlineServers)
    {
        ctx.proxy = server;
        ctx.nonceRequestHandle = server->apiConnection()->getNonceAsync(
            ctx.url,
            this,
            SLOT(at_getNonceForPingFinished(int, QnGetNonceReply, int, QString)));
        m_twoStepRequests[ctx.nonceRequestHandle] = ctx;
    }
}


int QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const nx::utils::Url &url, const QAuthenticator& userAuth, bool ownSettings)
{
    NX_LOG(lit("QnMergeSystemsTool: merge request to %1 url=%2")
            .arg(proxy->getApiUrl().toString(QUrl::RemovePassword))
            .arg(url.toString(QUrl::RemovePassword)),
        cl_logDEBUG1);

    TwoStepRequestCtx ctx;
    ctx.proxy = proxy;
    ctx.url = url;
    ctx.auth = userAuth;
    ctx.ownSettings = ownSettings;

    ctx.nonceRequestHandle = proxy->apiConnection()->
        getNonceAsync(url, this, SLOT(at_getNonceForMergeFinished(int, QnGetNonceReply, int, QString)));
    m_twoStepRequests[ctx.nonceRequestHandle] = ctx;
    return ctx.nonceRequestHandle;
}

void QnMergeSystemsTool::at_getNonceForMergeFinished(
    int /*status*/,
    const QnGetNonceReply& nonceReply,
    int handle,
    const QString& /*errorString*/)
{
    if (!m_twoStepRequests.contains(handle))
        return;

    TwoStepRequestCtx& ctx = m_twoStepRequests[handle];

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

    ctx.mainRequestHandle = ctx.proxy->apiConnection()->mergeSystemAsync(
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
    int /*status*/,
    const QnGetNonceReply& nonceReply,
    int handle,
    const QString& /*errorString*/)
{
    if (!m_twoStepRequests.contains(handle))
        return;

    TwoStepRequestCtx& ctx = m_twoStepRequests[handle];

    QByteArray getKey = createHttpQueryAuthParam(
        ctx.auth.user(),
        ctx.auth.password(),
        nonceReply.realm,
        "GET",
        nonceReply.nonce.toUtf8());

    ctx.mainRequestHandle = ctx.proxy->apiConnection()->pingSystemAsync(
        ctx.url,
        QString::fromLatin1(getKey),
        this,
        SLOT(at_pingSystem_finished(int, QnModuleInformation, int, QString)));
}

int QnMergeSystemsTool::configureIncompatibleServer(
    const QnMediaServerResourcePtr& proxy,
    const nx::utils::Url& url,
    const QAuthenticator& userAuth)
{
    TwoStepRequestCtx ctx;
    ctx.proxy = proxy;
    ctx.url = url;
    ctx.auth = userAuth;
    ctx.ownSettings = true;
    ctx.oneServer = true;
    ctx.ignoreIncompatible = true;

    ctx.nonceRequestHandle = proxy->apiConnection()->getNonceAsync(
        url, this, SLOT(at_getNonceForMergeFinished(int, QnGetNonceReply, int, QString)));
    m_twoStepRequests[ctx.nonceRequestHandle] = ctx;
    return ctx.nonceRequestHandle;
}

void QnMergeSystemsTool::at_pingSystem_finished(
    int status,
    const QnModuleInformation& moduleInformation,
    int handle,
    const QString& errorString)
{
    bool ctxFound = false;
    TwoStepRequestCtx ctx;
    for (const auto& value: m_twoStepRequests)
    {
        if (value.mainRequestHandle == handle)
        {
            handle = value.nonceRequestHandle;
            ctx = value;
            m_twoStepRequests.remove(handle);
            ctxFound = true;
            break;
        }
    }
    if (!ctxFound)
        return;

    NX_LOG(lit("QnMergeSystemsTool: ping response from %1 [%2 %3]")
        .arg(ctx.proxy->getApiUrl().toString()).arg(status).arg(errorString),
        cl_logDEBUG1);

    auto errorCode = (status == 0)
        ? utils::MergeSystemsStatus::fromString(errorString)
        : utils::MergeSystemsStatus::unknownError;

    auto isOk = [](utils::MergeSystemsStatus::Value errorCode)
        {
            return errorCode == utils::MergeSystemsStatus::ok
                || errorCode == utils::MergeSystemsStatus::starterLicense;
        };

    if (isOk(errorCode) && moduleInformation.ecDbReadOnly)
        errorCode = utils::MergeSystemsStatus::safeMode;

    if (isOk(errorCode))
    {
        m_twoStepRequests.clear();
        emit systemFound(errorCode, moduleInformation, ctx.proxy);
        return;
    }

    if (errorCode != utils::MergeSystemsStatus::notFound)
    {
        m_foundModule.first = errorCode;
        m_foundModule.second = moduleInformation;
    }

    if (m_twoStepRequests.isEmpty())
        emit systemFound(m_foundModule.first, m_foundModule.second, QnMediaServerResourcePtr());
}

void QnMergeSystemsTool::at_mergeSystem_finished(
    int status,
    const QnModuleInformation& moduleInformation,
    int handle,
    const QString& errorString)
{
    bool ctxFound = false;
    for (const auto& ctx: m_twoStepRequests)
    {
        if (ctx.mainRequestHandle == handle)
        {
            handle = ctx.nonceRequestHandle;
            m_twoStepRequests.remove(handle);
            ctxFound = true;
            break;
        }
    }
    if (!ctxFound)
        return;

    NX_LOG(lit("QnMergeSystemsTool: merge reply id=%1 error=%2")
        .arg(moduleInformation.id.toString()).arg(errorString), cl_logDEBUG1);

    auto errorCode = utils::MergeSystemsStatus::unknownError;
    if (status == QNetworkReply::ContentOperationNotPermittedError)
        errorCode = utils::MergeSystemsStatus::forbidden;
    else if (status == 0)
        errorCode = utils::MergeSystemsStatus::fromString(errorString);

    emit mergeFinished(errorCode, moduleInformation, handle);
}
