// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_image_provider.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrlQuery>
#include <QtQuick/QQuickImageResponse>

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::mobile {
namespace {

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

    virtual ~PushNotificationImageResponse() override
    {
        if (m_client)
            m_client->pleaseStopSync();
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

        auto statusWatcher = appContext()->cloudStatusWatcher();

        using namespace nx::cloud::db::api;
        IssueTokenRequest request{
            .grant_type = GrantType::refresh_token,
            .response_type = ResponseType::token,
            .scope = QString("cloudSystemId=%1").arg(cloudSystemId).toStdString(),
            .refresh_token = statusWatcher->remoteConnectionCredentials().authToken.value
        };

        auto cloudConnection = statusWatcher->cloudConnection();
        if (!cloudConnection)
            return callback({});

        cloudConnection->oauthManager()->issueTokenLegacy(request,
            [this, url, callback](ResultCode result, IssueTokenResponse response)
            {
                if (result != ResultCode::ok)
                    return callback({});

                using namespace nx::network;
                m_client = std::make_unique<http::AsyncClient>(ssl::kAcceptAnyCertificate);
                m_client->setCredentials(http::BearerAuthToken{response.access_token});
                m_client->doGet(url.toString(),
                    [this, callback]()
                    {
                        callback(m_client->fetchMessageBodyBuffer().toByteArray());
                    });
            });
    }

private:
    QImage m_image;
    std::unique_ptr<nx::network::http::AsyncClient> m_client;
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
