#pragma once

#include <QtCore/QScopedPointer>

#include "image_provider.h"

#include <nx/core/watermark/watermark.h>
#include <nx/utils/scoped_connections.h>

namespace nx::core { struct Watermark; }

namespace nx::vms::client::desktop {

/**
 * This image provider takes input from another ImageProvider
 * and (possibly) adds a watermark to it.
 */
class WatermarkProxyProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;
    using Watermark = nx::core::Watermark;

public:
    explicit WatermarkProxyProvider(QObject* parent = nullptr);
    explicit WatermarkProxyProvider(ImageProvider* sourceProvider, QObject* parent = nullptr);
    virtual ~WatermarkProxyProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    ImageProvider* sourceProvider() const;
    void setSourceProvider(ImageProvider* sourceProvider); //< Does not take ownership.

    void setWatermark(const Watermark& watermark);

protected:
    virtual void doLoadAsync() override;

private:
    void setImage(const QImage& image);

private:
    ImageProvider* m_sourceProvider = nullptr;
    nx::utils::ScopedConnections m_sourceProviderConnections;

    QImage m_image;
    Watermark m_watermark;
};

} // namespace nx::vms::client::desktop
