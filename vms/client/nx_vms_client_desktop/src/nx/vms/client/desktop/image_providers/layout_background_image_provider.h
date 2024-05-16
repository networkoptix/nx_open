// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/core/image_providers/image_provider.h>

namespace nx::vms::client::desktop {

class LayoutBackgroundImageProvider: public core::ImageProvider
{
    Q_OBJECT
    using base_type = core::ImageProvider;

public:
    explicit LayoutBackgroundImageProvider(const QnLayoutResourcePtr& layout,
        const QSize& maxImageSize = QSize(),
        QObject* parent = nullptr);
    virtual ~LayoutBackgroundImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual core::ThumbnailStatus status() const override;

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
