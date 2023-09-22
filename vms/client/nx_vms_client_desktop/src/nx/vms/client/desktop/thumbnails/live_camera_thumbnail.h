// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/thumbnails/abstract_caching_resource_thumbnail.h>
#include <nx/vms/client/core/thumbnails/camera_async_image_request.h>

namespace nx::vms::client::desktop {

/**
 * A thumbnail to display current image from a chosen camera stream. Refreshed only by request.
 * An image is cached and can be shared between several thumbnails; if one such thumbnail
 * requests a refresh, all thumbnails for the same camera/stream/size are refreshed.
 */
class LiveCameraThumbnail: public nx::vms::client::core::AbstractCachingResourceThumbnail
{
    Q_OBJECT
    Q_ENUMS(CameraStream)
    Q_PROPERTY(CameraStream stream READ stream WRITE setStream NOTIFY streamChanged)

    using base_type = nx::vms::client::core::AbstractCachingResourceThumbnail;

public:
    LiveCameraThumbnail(QObject* parent = nullptr);
    virtual ~LiveCameraThumbnail() override;

    using CameraStream = nx::vms::client::core::CameraAsyncImageRequest::CameraStream;

    CameraStream stream() const;
    void setStream(CameraStream value);

    static nx::vms::client::core::ThumbnailCache* thumbnailCache();

    static void registerQmlType();

signals:
    void streamChanged();

protected:
    virtual std::unique_ptr<nx::vms::client::core::AsyncImageResult> getImageAsyncUncached() const;
    virtual QString thumbnailId() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
