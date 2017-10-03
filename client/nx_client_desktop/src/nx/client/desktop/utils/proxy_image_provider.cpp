#include "proxy_image_provider.h"

#include <nx/utils/disconnect_helper.h>
#include <nx/client/desktop/utils/abstract_image_processor.h>

namespace nx {
namespace client {
namespace desktop {

ProxyImageProvider::ProxyImageProvider(QObject* parent):
    base_type(parent)
{
}

ProxyImageProvider::ProxyImageProvider(QnImageProvider* sourceProvider, QObject* parent):
    ProxyImageProvider(parent)
{
    setSourceProvider(sourceProvider);
}

ProxyImageProvider::~ProxyImageProvider()
{
}

QImage ProxyImageProvider::image() const
{
    return m_image;
}

QSize ProxyImageProvider::sizeHint() const
{
    return m_sizeHint;
}

Qn::ThumbnailStatus ProxyImageProvider::status() const
{
    return m_status;
}

QnImageProvider* ProxyImageProvider::sourceProvider() const
{
    return m_sourceProvider;
}

void ProxyImageProvider::setSourceProvider(QnImageProvider* sourceProvider)
{
    if (m_sourceProvider == sourceProvider)
        return;

    m_sourceProviderConnections.reset(new QnDisconnectHelper());
    m_sourceProvider = sourceProvider;

    if (m_sourceProvider)
    {
        *m_sourceProviderConnections << connect(m_sourceProvider, &QObject::destroyed,
            this, [this]() { setSourceProvider(nullptr); });

        *m_sourceProviderConnections << connect(m_sourceProvider, &QnImageProvider::statusChanged,
            this, &ProxyImageProvider::setStatus);

        *m_sourceProviderConnections << connect(m_sourceProvider, &QnImageProvider::sizeHintChanged,
            this, &ProxyImageProvider::setSourceSizeHint);

        *m_sourceProviderConnections << connect(m_sourceProvider, &QnImageProvider::imageChanged,
            this, &ProxyImageProvider::setSourceImage);

        setStatus(m_sourceProvider->status());
        setSourceSizeHint(m_sourceProvider->sizeHint());
        setSourceImage(m_sourceProvider->image());
    }
    else
    {
        setStatus(Qn::ThumbnailStatus::Invalid);
        setSizeHint(QSize());
        setImage(QImage());
    }
}

AbstractImageProcessor* ProxyImageProvider::imageProcessor() const
{
    return m_imageProcessor;
}

void ProxyImageProvider::setImageProcessor(AbstractImageProcessor* imageProcessor)
{
    if (m_imageProcessor == imageProcessor)
        return;

    m_imageProcessorConnections.reset(new QnDisconnectHelper());
    m_imageProcessor = imageProcessor;

    if (m_imageProcessor)
    {
        *m_imageProcessorConnections << connect(m_imageProcessor, &QObject::destroyed,
            this, [this]() { setImageProcessor(nullptr); });

        *m_imageProcessorConnections << connect(m_imageProcessor,
            &AbstractImageProcessor::updateRequired, this, &ProxyImageProvider::updateFromSource);
    }

    updateFromSource();
}

void ProxyImageProvider::doLoadAsync()
{
    if (m_sourceProvider)
        m_sourceProvider->loadAsync();
}

void ProxyImageProvider::setStatus(Qn::ThumbnailStatus status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(m_status);
}

void ProxyImageProvider::setSizeHint(const QSize& sizeHint)
{
    if (m_sizeHint == sizeHint)
        return;

    m_sizeHint = sizeHint;
    emit sizeHintChanged(m_sizeHint);
}

void ProxyImageProvider::setImage(const QImage& image)
{
    const bool changed = !(m_image.isNull() && image.isNull());
    m_image = image;

    if (changed)
        emit imageChanged(m_image);
}

void ProxyImageProvider::setSourceSizeHint(const QSize& sourceSizeHint)
{
    setSizeHint(m_imageProcessor ? m_imageProcessor->process(sourceSizeHint) : sourceSizeHint);
}

void ProxyImageProvider::setSourceImage(const QImage& sourceImage)
{
    setImage(m_imageProcessor ? m_imageProcessor->process(sourceImage) : sourceImage);
}

void ProxyImageProvider::updateFromSource()
{
    if (!m_sourceProvider)
        return;

    setSourceSizeHint(m_sourceProvider->sizeHint());
    setSourceImage(m_sourceProvider->image());
}

} // namespace desktop
} // namespace client
} // namespace nx
