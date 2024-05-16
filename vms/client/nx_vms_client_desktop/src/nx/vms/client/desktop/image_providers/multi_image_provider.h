// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtGui/QImage>

#include <nx/vms/client/core/image_providers/image_provider.h>

namespace nx::vms::client::desktop {

class MultiImageProvider: public core::ImageProvider
{
    Q_OBJECT
    using base_type = core::ImageProvider;

public:
    typedef std::vector<std::unique_ptr<ImageProvider>> Providers;

    MultiImageProvider(
        Providers providers,
        Qt::Orientation orientation,
        int spacing,
        QObject* parent = nullptr);
    virtual ~MultiImageProvider() override = default;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual core::ThumbnailStatus status() const override;

protected:
    virtual void doLoadAsync() override;

private:
    const Providers m_providers;
    const Qt::Orientation m_orientation = Qt::Vertical;
    const int m_spacing = 0;
    QMap<int, QRect> m_imageRects;
    QImage m_image;
};

} // namespace nx::vms::client::desktop
