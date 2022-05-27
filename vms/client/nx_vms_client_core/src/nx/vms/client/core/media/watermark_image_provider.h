// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickImageProvider>

#include <nx/core/watermark/watermark.h>

namespace nx::vms::client::core {

/** Provides watermark images for the cameras. */
class NX_VMS_CLIENT_CORE_API WatermarkImageProvider: public QQuickImageProvider
{
    using base_type = QQuickImageProvider;

public:
    WatermarkImageProvider();
    virtual ~WatermarkImageProvider() override;

    virtual QImage requestImage(
        const QString& request,
        QSize* size,
        const QSize& requestedSize) override;

    static QString name();

    /** Construct watermark source url with the specified parameters. */
    static QUrl makeWatermarkSourceUrl(
        const nx::core::Watermark& watermark,
        const QSize& size);
};

} // nx::vms::client::core
