// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/thumbnails/abstract_caching_resource_thumbnail.h>

namespace nx::vms::client::desktop {

/**
 * A thumbnail to display an image from a local file. Refreshed only by request.
 * An image is cached and can be shared between several thumbnails.
 * For local videos, a frame from the middle is loaded.
 */
class LocalMediaThumbnail: public nx::vms::client::core::AbstractCachingResourceThumbnail
{
    Q_OBJECT
    using base_type = nx::vms::client::core::AbstractCachingResourceThumbnail;

public:
    explicit LocalMediaThumbnail(QObject* parent = nullptr);
    virtual ~LocalMediaThumbnail() override;

    static void registerQmlType();

protected:
    virtual std::unique_ptr<nx::vms::client::core::AsyncImageResult> getImageAsyncUncached() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
