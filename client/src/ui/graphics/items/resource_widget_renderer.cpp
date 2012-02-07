#include "resource_widget_renderer.h"
#include <QMutexLocker>
#include <camera/gl_renderer.h>
#include <utils/common/warnings.h>
#include <utils/common/performance.h>

QnResourceWidgetRenderer::QnResourceWidgetRenderer(int channelCount, QObject *parent):
    QObject(parent)
{
    for(int i = 0; i < channelCount; i++)
        m_channelRenderers.push_back(new QnGLRenderer());
}

void QnResourceWidgetRenderer::beforeDestroy() {
    foreach(QnGLRenderer *renderer, m_channelRenderers)
        renderer->beforeDestroy();
}

QnResourceWidgetRenderer::~QnResourceWidgetRenderer() {
    foreach(QnGLRenderer *renderer, m_channelRenderers)
        delete renderer;

    m_channelRenderers.clear();
}

void QnResourceWidgetRenderer::update() {
    foreach(QnGLRenderer *renderer, m_channelRenderers)
        renderer->update();
}

qint64 QnResourceWidgetRenderer::lastDisplayedTime(int channel) const { 
    return m_channelRenderers[channel]->lastDisplayedTime();
}

qint64 QnResourceWidgetRenderer::isLowQualityImage(int channel) const { 
    return m_channelRenderers[channel]->isLowQualityImage();
}

QnMetaDataV1Ptr QnResourceWidgetRenderer::lastFrameMetadata(int channel) const 
{ 
    return m_channelRenderers[channel]->lastFrameMetadata();
}

QnResourceWidgetRenderer::RenderStatus QnResourceWidgetRenderer::paint(int channel, const QRectF &rect, qreal opacity) {
    frameDisplayed();

    QnGLRenderer *renderer = m_channelRenderers[channel];
    renderer->setOpacity(opacity);
    return renderer->paint(rect);
}

void QnResourceWidgetRenderer::draw(CLVideoDecoderOutput *image) {
    m_channelRenderers[image->channel]->draw(image);

    QSize sourceSize = QSize(image->width * image->sample_aspect_ratio, image->height);
    
    if(m_sourceSize != sourceSize) {
        m_sourceSize = sourceSize;
        emit sourceSizeChanged(sourceSize);
    }
}

void QnResourceWidgetRenderer::waitForFrameDisplayed(int channel) {
    m_channelRenderers[channel]->waitForFrameDisplayed(channel);
}

QSize QnResourceWidgetRenderer::sizeOnScreen(unsigned int /*channel*/) const {
    QMutexLocker locker(&m_mutex);
    
    return m_channelScreenSize;
}

void QnResourceWidgetRenderer::setChannelScreenSize(const QSize &screenSize) {
    QMutexLocker locker(&m_mutex);

    m_channelScreenSize = screenSize;
}

bool QnResourceWidgetRenderer::constantDownscaleFactor() const {
    return false;
}
