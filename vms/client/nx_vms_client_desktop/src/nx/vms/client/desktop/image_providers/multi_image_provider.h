#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtGui/QImage>

#include "image_provider.h"

namespace nx::vms::client::desktop {

class MultiImageProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

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
    virtual Qn::ThumbnailStatus status() const override;

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
