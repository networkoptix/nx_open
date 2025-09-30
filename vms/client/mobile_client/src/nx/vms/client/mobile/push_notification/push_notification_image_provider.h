// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::mobile {

class PushNotificationImageProvider: public QQuickImageProvider
{
public:
    static constexpr auto id = "push_notification";

    PushNotificationImageProvider();
    virtual QImage requestImage(const QString &imageId, QSize*, const QSize&) override;

    static QString url(const QString& imageId);
};

} // namespace nx::vms::client::mobile
