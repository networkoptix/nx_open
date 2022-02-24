// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/thumbnails/abstract_resource_thumbnail.h>

namespace nx::vms::client::desktop::workbench::timeline {

class ThumbnailLoadingManager;

class LivePreviewThumbnail: public nx::vms::client::core::AbstractResourceThumbnail
{
    Q_OBJECT
    using base_type = nx::vms::client::core::AbstractResourceThumbnail;

public:
    explicit LivePreviewThumbnail(QObject* parent = nullptr);
    virtual ~LivePreviewThumbnail() override;

    ThumbnailLoadingManager* thumbnailLoadingManager() const;
    void setThumbnailLoadingManager(ThumbnailLoadingManager* value);

    std::chrono::milliseconds timestamp() const;
    void setTimestamp(std::chrono::milliseconds value);

public:
    static constexpr std::chrono::milliseconds kNoTimestamp{-1};

protected:
    virtual std::unique_ptr<nx::vms::client::core::AsyncImageResult> getImageAsync(
        bool /*forceRefresh*/) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::workbench::timeline
