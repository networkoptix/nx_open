#include "display_widget_renderer.h"
#include <QMutexLocker>
#include <camera/gl_renderer.h>
#include <utils/common/warnings.h>

QnDisplayWidgetRenderer::QnDisplayWidgetRenderer(int channelCount, QObject *parent):
    QObject(parent),
    m_decodingThread(NULL)
{
    for(int i = 0; i < channelCount; i++)
        m_channelRenderers.push_back(new CLGLRenderer());
}

void QnDisplayWidgetRenderer::beforeDestroy() {
    checkThread(false);

    foreach(CLGLRenderer *renderer, m_channelRenderers)
        renderer->beforeDestroy();
}

QnDisplayWidgetRenderer::~QnDisplayWidgetRenderer() {
    checkThread(false);

    foreach(CLGLRenderer *renderer, m_channelRenderers)
        delete renderer;

    m_channelRenderers.clear();
}

bool QnDisplayWidgetRenderer::paint(int channel, const QRectF &rect) {
    checkThread(false);

    return m_channelRenderers[channel]->paintEvent(rect);
}

void QnDisplayWidgetRenderer::checkThread(bool inDecodingThread) const {
#ifdef _DEBUG________________
    if(inDecodingThread) {
        if(m_decodingThread == NULL) {
            m_decodingThread = QThread::currentThread();
            return;
        }

        if(m_decodingThread != QThread::currentThread())
            qnWarning("This function is supposed to be called from decoding thread only.");
    } else {
        if(thread() != QThread::currentThread())
            qnWarning("This function is supposed to be called from GUI thread only.");
    }
#endif
}

void QnDisplayWidgetRenderer::draw(CLVideoDecoderOutput *image) {
    checkThread(true);

    m_channelRenderers[image->channel]->draw(image);

    QSize sourceSize = QSize(image->width * image->sample_aspect_ratio, image->height);
    
    if(m_sourceSize != sourceSize) {
        m_sourceSize = sourceSize;
        emit sourceSizeChanged(sourceSize);
    }
}

void QnDisplayWidgetRenderer::waitForFrameDisplayed(int channel) {
    checkThread(true);

    m_channelRenderers[channel]->waitForFrameDisplayed(channel);
}

QSize QnDisplayWidgetRenderer::sizeOnScreen(unsigned int /*channel*/) const {
    checkThread(true);

    QMutexLocker locker(&m_mutex);
    
    return m_channelScreenSize;
}

void QnDisplayWidgetRenderer::setChannelScreenSize(const QSize &screenSize) {
    QMutexLocker locker(&m_mutex);

    m_channelScreenSize = screenSize;
}

bool QnDisplayWidgetRenderer::constantDownscaleFactor() const {
    checkThread(true);

    return false;
}
