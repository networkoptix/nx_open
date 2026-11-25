// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_image_provider.h"

#include <chrono>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtQuick/QQuickImageResponse>

#include <api/server_rest_connection.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::mobile {
namespace {

using namespace std::chrono;

using ImageHandler = std::function<void(QByteArray data)>;

/**
 * Loads the image through the server connection of the cross-site context of the cloud system,
 * so the connection authorizes the request. Waits until the context appears and connects, or
 * renews the expired token, but not longer than the timeout. Gives empty data if the image can't
 * be loaded.
 */
class CrossSystemImageRequest: public QObject
{
public:
    static void start(const QString& cloudSystemId, const QUrl& url, ImageHandler handler)
    {
        const auto manager = appContext()->cloudCrossSystemManager();
        if (!manager)
        {
            NX_DEBUG(
                typeid(CrossSystemImageRequest), "Cannot load %1: no cross-site contexts", url);
            return handler({});
        }

        // Sites over the limit of parallel connections connect one by one, so the site of the
        // image goes first.
        manager->setPriority(cloudSystemId, core::CloudCrossSystemManager::Priority::high);

        // The request is destroyed with the manager, then the handler is not called.
        new CrossSystemImageRequest(manager, cloudSystemId, url, std::move(handler));
    }

private:
    CrossSystemImageRequest(core::CloudCrossSystemManager* manager,
        const QString& cloudSystemId,
        const QUrl& url,
        ImageHandler handler):
        QObject(manager),
        m_manager(manager),
        m_cloudSystemId(cloudSystemId),
        m_url(url),
        m_handler(std::move(handler))
    {
        static constexpr auto kTimeout = 30s;
        QTimer::singleShot(kTimeout, this, [this]() { fail("timed out"); });

        connect(manager,
            &core::CloudCrossSystemManager::systemFound,
            this,
            [this](const QString& systemId)
            {
                if (systemId == m_cloudSystemId)
                    watchContext();
            });

        connect(manager,
            &core::CloudCrossSystemManager::systemLost,
            this,
            [this](const QString& systemId)
            {
                if (systemId == m_cloudSystemId)
                    fail("the site is lost");
            });

        watchContext();
    }

    void watchContext()
    {
        const auto context = m_manager->systemContext(m_cloudSystemId);
        if (!context || trySend(context))
            return;

        // The system context is deleted later than the cross-site context.
        connect(context->systemContext(),
            &core::SystemContext::credentialsChanged,
            this,
            [this, context = QPointer(context)]()
            {
                if (context)
                    trySend(context);
            });

        connect(context,
            &core::CloudCrossSystemContext::statusChanged,
            this,
            [this, context]() { trySend(context); });

        // Connect now instead of waiting for the next attempt of the context. Does nothing if
        // the context is already connected or connecting.
        context->initializeConnection(/*allowUserInteraction*/ false);
    }

    bool trySend(core::CloudCrossSystemContext* context)
    {
        if (m_handle != rest::Handle() || !m_handler)
            return true;

        const auto connection = context->systemContext()->connection();
        if (connection && !isTokenExpired(*connection))
        {
            send(context->systemContext()->connectedServerApi());
            return true;
        }

        using Status = core::CloudCrossSystemContext::Status;
        const auto status = context->status();
        const bool isOnline = context->systemDescription()->isOnline();
        if (status == Status::unsupportedPermanently || status == Status::unsupportedTemporary
            || !isOnline)
        {
            fail(
                nx::format("the site status is %1, online: %2", core::toString(status), isOnline));
            return true;
        }

        return false;
    }

    static bool isTokenExpired(const core::RemoteConnection& connection)
    {
        const auto expirationTime = connection.sessionTokenExpirationTime();
        return expirationTime && *expirationTime <= qnSyncTime->currentTimePoint();
    }

    void send(const rest::ServerConnectionPtr& api)
    {
        if (!api)
            return fail("no server API");

        m_handle = api->getRawResult(
            m_url.path(),
            nx::network::rest::Params::fromUrlQuery(QUrlQuery(m_url)),
            [this](bool success, rest::Handle, QByteArray data, const auto& /*headers*/)
            {
                if (success)
                    finish(std::move(data));
                else
                    fail(nx::format("the request failed: %1", QString::fromUtf8(data.left(50))));
            },
            this);

        if (m_handle == rest::Handle())
            fail("the request is not sent");
    }

    void fail(const QString& reason)
    {
        if (!m_handler)
            return;

        NX_DEBUG(this, "Cannot load %1: %2", m_url, reason);
        finish({});
    }

    void finish(QByteArray data)
    {
        if (!m_handler)
            return;

        std::exchange(m_handler, {})(std::move(data));
        deleteLater();
    }

    core::CloudCrossSystemManager* const m_manager;
    const QString m_cloudSystemId;
    const QUrl m_url;
    ImageHandler m_handler;
    rest::Handle m_handle{};
};

class PushNotificationImageResponse: public QQuickImageResponse
{
public:
    PushNotificationImageResponse(const QString& cloudSystemId, const QString& imageUrl)
    {
        if (auto data = appContext()->pushNotificationStorage()->findImage(imageUrl.toStdString()))
            m_image.loadFromData(reinterpret_cast<const uchar*>(data->data()), data->size());

        if (!m_image.isNull())
        {
            emit finished();
            return;
        }

        load(cloudSystemId, imageUrl, threadGuarded(
            [this, imageUrl](const QByteArray& data)
            {
                if (m_image.loadFromData(data))
                {
                    const auto ptr = reinterpret_cast<const std::byte*>(data.data());
                    std::vector<std::byte> result{ptr, ptr + data.length()};
                    appContext()->pushNotificationStorage()->saveImage(
                        imageUrl.toStdString(), result);
                }

                emit finished();
            }));
    }

    virtual QQuickTextureFactory* textureFactory() const override
    {
        return !m_image.isNull() ? QQuickTextureFactory::textureFactoryForImage(m_image) : nullptr;
    }

    virtual QString errorString() const override
    {
        return m_image.isNull() ? "No image" : QString{};
    }

    void load(
        const QString& cloudSystemId,
        const QUrl& url,
        std::function<void(const QByteArray&)> callback)
    {
        if (!NX_ASSERT(callback))
            return;

        if (!url.isValid())
            return callback({});

        auto handler = nx::utils::AsyncHandlerExecutor(this).bind(std::move(callback));

        const nx::utils::AsyncHandlerExecutor mainThread(appContext());
        mainThread.submit([cloudSystemId, url, handler = std::move(handler)]() mutable
            { CrossSystemImageRequest::start(cloudSystemId, url, std::move(handler)); });
    }

private:
    QImage m_image;
};

}

PushNotificationImageProvider::PushNotificationImageProvider():
    QQuickAsyncImageProvider()
{
}

QQuickImageResponse* PushNotificationImageProvider::requestImageResponse(
    const QString& id, const QSize&)
{
    const auto cloudSystemId = id.first(id.indexOf("/"));
    const auto imageUrl = id.mid(cloudSystemId.length() + 1);
    return new PushNotificationImageResponse(cloudSystemId, imageUrl);
}

QString PushNotificationImageProvider::url(const QString& cloudSystemId, const QString& imageUrl)
{
    return QString("image://%1/%2/%3").arg(id, cloudSystemId, imageUrl);
}

} // namespace nx::vms::client::mobile
