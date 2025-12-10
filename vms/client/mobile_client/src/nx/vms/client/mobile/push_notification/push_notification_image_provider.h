// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickAsyncImageProvider>

namespace nx::vms::client::mobile {

class PushNotificationImageProvider: public QQuickAsyncImageProvider
{
public:
    static constexpr auto id = "push_notification";

    PushNotificationImageProvider();

    virtual QQuickImageResponse* requestImageResponse(const QString& id, const QSize&) override;

    static QString url(const QString& cloudSystemId, const QString& imageUrl);
};

} // namespace nx::vms::client::mobile
