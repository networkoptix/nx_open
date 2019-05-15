#include "watermark_proxy_provider.h"

#include <QtGui/QPainter>

#include <nx/vms/client/desktop/watermark/watermark_painter.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

WatermarkProxyProvider::WatermarkProxyProvider(QObject* parent):
    base_type(parent)
{
}

WatermarkProxyProvider::WatermarkProxyProvider(ImageProvider* sourceProvider, QObject* parent):
    WatermarkProxyProvider(parent)
{
    setSourceProvider(sourceProvider);
}

WatermarkProxyProvider::~WatermarkProxyProvider()
{
}

QImage WatermarkProxyProvider::image() const
{
    if (m_image.isNull())
        return m_sourceProvider->image();

    return m_image;
}

QSize WatermarkProxyProvider::sizeHint() const
{
    NX_ASSERT(m_sourceProvider);
    return m_sourceProvider->sizeHint();
}

Qn::ThumbnailStatus WatermarkProxyProvider::status() const
{
    NX_ASSERT(m_sourceProvider);
    return m_sourceProvider->status();
}

ImageProvider* WatermarkProxyProvider::sourceProvider() const
{
    return m_sourceProvider;
}

void WatermarkProxyProvider::setSourceProvider(ImageProvider* sourceProvider)
{
    if (m_sourceProvider == sourceProvider)
        return;

    m_sourceProviderConnections = {};
    m_sourceProvider = sourceProvider;

    if (m_sourceProvider)
    {
        m_sourceProviderConnections << connect(m_sourceProvider, &QObject::destroyed,
            this, [this]() { setSourceProvider(nullptr); });

        m_sourceProviderConnections << connect(m_sourceProvider, &ImageProvider::statusChanged,
            this, &WatermarkProxyProvider::statusChanged);

        m_sourceProviderConnections << connect(m_sourceProvider, &ImageProvider::sizeHintChanged,
            this, &WatermarkProxyProvider::sizeHintChanged);

        m_sourceProviderConnections << connect(m_sourceProvider, &ImageProvider::imageChanged,
            this, &WatermarkProxyProvider::setImage);

        setImage(m_sourceProvider->image());
    }
    else
        setImage(QImage());
}

void WatermarkProxyProvider::setWatermark(const Watermark& watermark)
{
    m_watermark = watermark;
}

void WatermarkProxyProvider::doLoadAsync()
{
    if (m_sourceProvider)
        m_sourceProvider->loadAsync();
}

void WatermarkProxyProvider::setImage(const QImage& image)
{
    if (m_image.isNull() && image.isNull())
        return; //< Nothing changed

    m_image = image;

    if (!m_image.size().isEmpty() && m_watermark.visible()) //< Actually draw watermark.
    {
        WatermarkPainter painter;
        painter.setWatermark(m_watermark);
        QPainter qtPainter(&m_image);
        painter.drawWatermark(&qtPainter, m_image.rect());
    }

    emit sizeHintChanged(m_image.size());
    emit imageChanged(m_image);
}

} // namespace nx::vms::client::desktop
