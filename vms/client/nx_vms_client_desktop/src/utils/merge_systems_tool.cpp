#include "merge_systems_tool.h"

#include <QtCore/QTimer>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkReply>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "api/app_server_connection.h"
#include "api/server_rest_connection.h"
#include "nx_ec/dummy_handler.h"
#include "common/common_module.h"
#include "nx/vms/discovery/manager.h"
#include <nx/utils/log/log.h>
#include <nx/utils/guarded_callback.h>
#include "client/client_settings.h"
#include <network/authutil.h>
#include <nx/network/deprecated/asynchttpclient.h>

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

        NX_DEBUG(this, lm("pingSystem(): Request nonce from %1").arg(ctx.peerString()));

        auto callback = [this](
                bool success, int handle,
                rest::RestResultWithData<QnGetNonceReply> reply)
            {
                at_getNonceForPingFinished(success, handle, reply.data, reply.errorString);
            };
        ctx.nonceRequestHandle = server->restConnection()->getNonceAsync(ctx.url,
            nx::utils::guarded(this, callback), thread());
        m_twoStepRequests[ctx.nonceRequestHandle] = ctx;
    }
}

int QnMergeSystemsTool::mergeSystem(const QnMediaServerResourcePtr &proxy, const nx::utils::Url &url, const QAuthenticator& userAuth, bool ownSettings)
{
    TwoStepRequestCtx ctx;
    ctx.proxy = proxy;
    ctx.url = url;
    ctx.auth = userAuth;
    ctx.ownSettings = ownSettings;

    NX_DEBUG(this, lm("mergeSystem(): Request nonce from %1").arg(ctx.peerString()));

    auto callback = [this](bool success, int handle, rest::RestResultWithData<QnGetNonceReply> reply)
        {
            at_getNonceForMergeFinished(success, handle, reply.data, reply.errorString);
        };
    ctx.nonceRequestHandle = proxy->restConnection()->getNonceAsync(
        url, nx::utils::guarded(this, callback), thread());
    m_twoStepRequests[ctx.nonceRequestHandle] = ctx;
    return ctx.nonceRequestHandle;
}

void QnMergeSystemsTool::at_getNonceForMergeFinished(
    [[maybe_unused]]bool success,
    int handle,
    const QnGetNonceReply& nonceReply,
    [[maybe_unused]]const QString& errorString)
{
    if (!m_twoStepRequests.contains(handle))
        return;

    TwoStepRequestCtx& ctx = m_twoStepRequests[handle];

    QString getKey = QString::fromLatin1(createHttpQueryAuthParam(
        ctx.auth.user(),
        ctx.auth.password(),
        nonceReply.realm,
        "GET",
        nonceReply.nonce.toUtf8()));

    QString postKey = QString::fromLatin1(createHttpQueryAuthParam(
        ctx.auth.user(),
        ctx.auth.password(),
        nonceReply.realm,
        "POST",
        nonceReply.nonce.toUtf8()));

    NX_DEBUG(this, lm("Send merge request to %1").arg(ctx.peerString()));

    auto callback =
        [this](bool success, rest::Handle handle, QnJsonRestResult response)
        {
            QString error = response.errorString;
            const auto moduleInfo = response.deserialized<nx::vms::api::ModuleInformation>();
            at_mergeSystem_finished(success, handle, moduleInfo, error);
        };

    if (auto connection = ctx.proxy->restConnection())
    {
        ctx.mainRequestHandle = connection->mergeSystemAsync(
            ctx.url, getKey, postKey,
            ctx.ownSettings, ctx.oneServer, ctx.ignoreIncompatible,
            callback, this->thread());
    }
}

void QnMergeSystemsTool::at_getNonceForPingFinished(
    [[maybe_unused]] bool success,
    int handle,
    const QnGetNonceReply& nonceReply,
    [[maybe_unused]]const QString& errorString)
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

    NX_DEBUG(this, lm("Send ping request to %1").arg(ctx.peerString()));

    auto callback =
        [this](bool success, rest::Handle handle,
            rest::RestResultWithData<nx::vms::api::ModuleInformation> response)
        {
            at_pingSystem_finished(success, handle, response.data, response.errorString);
        };

    ctx.mainRequestHandle = ctx.proxy->restConnection()->pingSystemAsync(
        ctx.url,
        QString::fromLatin1(getKey),
        nx::utils::guarded(this, callback), thread());
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

    NX_DEBUG(this,
        lm("configureIncompatibleServer(): Request nonce from %1").arg(ctx.peerString()));

    auto callback = [this](bool success, int handle,
            rest::RestResultWithData<QnGetNonceReply> reply)
        {
            at_getNonceForMergeFinished(success, handle, reply.data, reply.errorString);
        };
    ctx.nonceRequestHandle = proxy->restConnection()->getNonceAsync(
        url, nx::utils::guarded(this, callback), thread());
    m_twoStepRequests[ctx.nonceRequestHandle] = ctx;
    return ctx.nonceRequestHandle;
}

void QnMergeSystemsTool::at_pingSystem_finished(
    bool success,
    int handle,
    const nx::vms::api::ModuleInformation& moduleInformation,
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

    NX_DEBUG(this, lm("Ping response from %1: %2 %3").args(
        ctx.peerString(), success, errorString));

    auto errorCode = success
        ? utils::MergeSystemsStatus::unknownError
        : utils::MergeSystemsStatus::fromString(errorString);

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
    bool success,
    int handle,
    const nx::vms::api::ModuleInformation& moduleInformation,
    const QString& errorString)
{
    bool ctxFound = false;
    QString peerString;
    for (const auto& ctx: m_twoStepRequests)
    {
        if (ctx.mainRequestHandle == handle)
        {
            peerString = ctx.peerString();
            handle = ctx.nonceRequestHandle;
            m_twoStepRequests.remove(handle);
            ctxFound = true;
            break;
        }
    }
    if (!ctxFound)
        return;

    NX_DEBUG(this, lm("Merge response from %1: id=%2 error=%3").args(
        peerString, moduleInformation.id, errorString));

    auto errorCode = utils::MergeSystemsStatus::ok;
    if (!success)
        errorCode = utils::MergeSystemsStatus::unknownError;
    if (!errorString.isEmpty())
        errorCode = utils::MergeSystemsStatus::fromString(errorString);
    emit mergeFinished(errorCode, moduleInformation, handle);
}

QString QnMergeSystemsTool::TwoStepRequestCtx::peerString() const
{
    QString result = url.toString(QUrl::RemovePassword);
    if (proxy)
        result += lit(" (via %1)").arg(proxy->getApiUrl().toString(QUrl::RemovePassword));

    return result;
}
