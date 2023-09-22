// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/thumbnails/abstract_caching_resource_thumbnail.h>

namespace nx::vms::client::desktop {

/**
 * A thumbnail for resource identification. Currently supports cameras and local media files.
 * Camera stream is not relevant.
 *
 * Live camera frame is preferred, falling back to the last archive frame if present.
 * Camera thumbnails are periodically refreshed if camera is recording (easy for the server).
 * For a local video file thumbnail, the key frame closest to the middle is loaded.
 */
class ResourceIdentificationThumbnail:
    public nx::vms::client::core::AbstractCachingResourceThumbnail
{
    Q_OBJECT
    Q_PROPERTY(int refreshIntervalSeconds
        READ refreshIntervalSeconds WRITE setRefreshIntervalSeconds NOTIFY refreshIntervalChanged)
    Q_PROPERTY(bool enforced READ enforced WRITE setEnforced NOTIFY enforcedChanged)

    using base_type = nx::vms::client::core::AbstractCachingResourceThumbnail;

public:
    explicit ResourceIdentificationThumbnail(QObject* parent = nullptr);
    virtual ~ResourceIdentificationThumbnail() override;

    /**
     * If refresh interval is zero or negative, the automatic refresh is off.
     * Currently timed refresh is relevant only for cameras.
     */
    std::chrono::seconds refreshInterval() const;
    void setRefreshInterval(std::chrono::seconds value);
    int refreshIntervalSeconds() const { return refreshInterval().count(); }
    void setRefreshIntervalSeconds(int value) { setRefreshInterval(std::chrono::seconds(value)); }

    /** Whether preview loading is enforced for non-recording cameras. */
    bool enforced() const;
    void setEnforced(bool value);

    Q_INVOKABLE void setFallbackImage(const QUrl& imageUrl, qint64 timestampMs,
        int rotationQuadrants);

    static nx::vms::client::core::ThumbnailCache* thumbnailCache();

    static void registerQmlType();

signals:
    void refreshIntervalChanged();
    void enforcedChanged();

protected:
    virtual std::unique_ptr<nx::vms::client::core::AsyncImageResult>
        getImageAsync(bool forceRefresh) const override;

    virtual std::unique_ptr<nx::vms::client::core::AsyncImageResult>
        getImageAsyncUncached() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
