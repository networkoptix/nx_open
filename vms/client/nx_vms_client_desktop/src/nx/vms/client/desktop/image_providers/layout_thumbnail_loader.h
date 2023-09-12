// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <api/helpers/thumbnail_request_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

#include "image_provider.h"

namespace nx::core { struct Watermark; }

namespace nx::vms::client::desktop {

class LayoutThumbnailLoader:
    public ImageProvider,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    explicit LayoutThumbnailLoader(
        const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch,
        bool skipExportPermissionCheck = false,
        QObject* parent = nullptr);

    virtual ~LayoutThumbnailLoader() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnLayoutResourcePtr layout() const;

    void setWatermark(const nx::core::Watermark& watermark);

    QColor itemBackgroundColor() const;
    void setItemBackgroundColor(const QColor& value);
    void setRequestRoundMethod(nx::api::ImageRequest::RoundMethod roundMethod);

    /**
     * Whether to use tolerant mode when fetching camera thumbnails, i.e. allow fetching
     * the nearest available frame when there's no archive at the requested time.
     */
    void setTolerant(bool value);

    QColor fontColor() const;
    void setFontColor(const QColor& value);

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
