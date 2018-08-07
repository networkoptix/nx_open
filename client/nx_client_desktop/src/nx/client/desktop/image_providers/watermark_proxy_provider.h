#pragma once

#include <QtCore/QScopedPointer>

#include "image_provider.h"

#include <nx/core/watermark/watermark.h>


class QnDisconnectHelper;

namespace nx {

namespace core { struct Watermark; }

namespace client {
namespace desktop {

/**
 * This image provider takes input from another ImageProvider
 * and (possibly) adds a watermark to it.
 */
class WatermarkProxyProvider: public QnImageProvider
{
    Q_OBJECT
    using base_type = QnImageProvider;
    using Watermark = nx::core::Watermark;

public:
    explicit WatermarkProxyProvider(QObject* parent = nullptr);
    explicit WatermarkProxyProvider(QnImageProvider* sourceProvider, QObject* parent = nullptr);
    virtual ~WatermarkProxyProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnImageProvider* sourceProvider() const;
    void setSourceProvider(QnImageProvider* sourceProvider); //< Does not take ownership.

    void setWatermark(const Watermark& watermark);

protected:
    virtual void doLoadAsync() override;

private:
    void setImage(const QImage& image);

private:
    QnImageProvider* m_sourceProvider = nullptr;
    QScopedPointer<QnDisconnectHelper> m_sourceProviderConnections;

    QImage m_image;
    Watermark m_watermark;
};

} // namespace desktop
} // namespace client
} // namespace nx
