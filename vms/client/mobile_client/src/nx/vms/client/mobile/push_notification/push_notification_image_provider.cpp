// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_image_provider.h"

#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/details/secure_storage.h>

namespace nx::vms::client::mobile {

PushNotificationImageProvider::PushNotificationImageProvider():
    QQuickImageProvider(ImageType::Image)
{
}

QImage PushNotificationImageProvider::requestImage(const QString& imageId, QSize*, const QSize&)
{
    if (auto data = appContext()->secureStorage()->loadImage(imageId.toStdString()))
    {
        QImage result;
        result.loadFromData(reinterpret_cast<const uchar*>(data->data()), data->size());
        return result;
    }

    return {};
}

QString PushNotificationImageProvider::url(const QString& imageId)
{
    return QString("image://%1/%2").arg(id, imageId);
}

} // namespace nx::vms::client::mobile
