#include "statistics_widget_renderer.h"

#include <QtCore/QMutexLocker>

#include <camera/gl_renderer.h>
#include <utils/common/warnings.h>
#include <utils/common/performance.h>
#include <utils/settings.h>

QnStatisticsWidgetRenderer::QnStatisticsWidgetRenderer(int channelCount, QObject *parent, const QGLContext *context):
    QnResourceWidgetRenderer(channelCount, parent, context)
{
    for(int i = 0; i < channelCount; i++) {
        QnGLRenderer* renderer = new QnGLRenderer(context);
        renderer->setForceSoftYUV(qnSettings->isSoftwareYuv());
        m_channelRenderers.push_back(renderer);
    }
}

void QnStatisticsWidgetRenderer::beforeDestroy() {
    foreach(QnGLRenderer *renderer, m_channelRenderers)
        renderer->beforeDestroy();
}

QnStatisticsWidgetRenderer::~QnStatisticsWidgetRenderer() {
    foreach(QnGLRenderer *renderer, m_channelRenderers)
        delete renderer;

    m_channelRenderers.clear();
}

void QnStatisticsWidgetRenderer::update() {
    foreach(QnGLRenderer *renderer, m_channelRenderers)
        renderer->update();
}

qint64 QnStatisticsWidgetRenderer::lastDisplayedTime(int channel) const { 
    return m_channelRenderers[channel]->lastDisplayedTime();
}

qint64 QnStatisticsWidgetRenderer::isLowQualityImage(int channel) const { 
    return m_channelRenderers[channel]->isLowQualityImage();
}

QnMetaDataV1Ptr QnStatisticsWidgetRenderer::lastFrameMetadata(int channel) const 
{ 
    return m_channelRenderers[channel]->lastFrameMetadata(channel);
}

Qn::RenderStatus QnStatisticsWidgetRenderer::paint(int channel, const QRectF &rect, qreal opacity) {
    frameDisplayed();

    QnGLRenderer *renderer = m_channelRenderers[channel];
    renderer->setOpacity(opacity);
    return renderer->paint(rect);
}

void QnStatisticsWidgetRenderer::draw(CLVideoDecoderOutput *image) {
    m_channelRenderers[image->channel]->draw(image);

    QSize sourceSize = QSize(image->width * image->sample_aspect_ratio, image->height);
    
    if(m_sourceSize != sourceSize) {
        m_sourceSize = sourceSize;
        emit sourceSizeChanged(sourceSize);
    }
}

void QnStatisticsWidgetRenderer::waitForFrameDisplayed(int channel) {
    m_channelRenderers[channel]->waitForFrameDisplayed(channel);
}

QSize QnStatisticsWidgetRenderer::sizeOnScreen(unsigned int /*channel*/) const {
    QMutexLocker locker(&m_mutex);
    
    return m_channelScreenSize;
}

void QnStatisticsWidgetRenderer::setChannelScreenSize(const QSize &screenSize) {
    QMutexLocker locker(&m_mutex);

    m_channelScreenSize = screenSize;
}

bool QnStatisticsWidgetRenderer::constantDownscaleFactor() const {
    return false;
}

QSize QnStatisticsWidgetRenderer::sourceSize() const {
    QMutexLocker locker(&m_mutex);

    return m_sourceSize;
}

