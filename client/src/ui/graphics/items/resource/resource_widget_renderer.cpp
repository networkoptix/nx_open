#include "resource_widget_renderer.h"

#include <QtCore/QMutexLocker>

#include <camera/gl_renderer.h>
#include <utils/common/warnings.h>
#include <utils/common/performance.h>
#include <utils/settings.h>


QnResourceWidgetRenderer::QnResourceWidgetRenderer(
    int channelCount,
    QObject* parent,
    const QGLContext* context )
:
    QObject( parent ),
    m_glContext( context )
{
    if( !channelCount )
        return;

    Q_ASSERT( context != NULL );
    QSharedPointer<QGLContext> auxiliaryGLContext( new QGLContext( context->format(), context->device() ) );
    if( !auxiliaryGLContext->create( context ) )
    {
        cl_log.log( QString::fromAscii("QnResourceWidgetRenderer. Failed to create auxiliary opengl context shared with GUI thread context. "
            "Rendering of stream is impossible" ), cl_logERROR );
        return;
    }

    m_channelRenderers.resize( channelCount );
    for( int i = 0; i < channelCount; ++i )
    {
        RenderingTools renderingTools;
        renderingTools.uploader = new DecodedPictureToOpenGLUploader( auxiliaryGLContext );
        renderingTools.uploader->setForceSoftYUV( qnSettings->isSoftwareYuv() );
        renderingTools.renderer = new QnGLRenderer( context, *renderingTools.uploader );
        //renderingTools.uploader->setYV12ToRgbShaderUsed(renderingTools.renderer->isYV12ToRgbShaderUsed())
        //renderingTools.uploader->setNV12ToRgbShaderUsed(renderingTools.renderer->isNV12ToRgbShaderUsed())
        m_channelRenderers[i] = renderingTools;
    }
}

void QnResourceWidgetRenderer::beforeDestroy() {
    foreach(RenderingTools ctx, m_channelRenderers)
    {
        if( ctx.renderer )
            ctx.renderer->beforeDestroy();
        if( ctx.uploader )
            ctx.uploader->beforeDestroy();
    }
}

QnResourceWidgetRenderer::~QnResourceWidgetRenderer() {
    foreach(RenderingTools ctx, m_channelRenderers)
    {
        delete ctx.renderer;
        delete ctx.uploader;
    }

    m_channelRenderers.clear();
}

void QnResourceWidgetRenderer::update() {
    //renderer->update() is not needed anymore since during paint renderer takes newest decoded picture

    //foreach(RenderingTools ctx, m_channelRenderers)
    //{
    //    if( ctx.renderer )
    //        ctx.renderer->update();
    //}
}

qint64 QnResourceWidgetRenderer::lastDisplayedTime(int channel) const { 
    const RenderingTools& ctx = m_channelRenderers[channel];
    return ctx.renderer ? ctx.renderer->lastDisplayedTime() : 0;
}

qint64 QnResourceWidgetRenderer::isLowQualityImage(int channel) const { 
    const RenderingTools& ctx = m_channelRenderers[channel];
    return ctx.renderer ? ctx.renderer->isLowQualityImage() : 0;
}

QnMetaDataV1Ptr QnResourceWidgetRenderer::lastFrameMetadata(int channel) const 
{ 
    const RenderingTools& ctx = m_channelRenderers[channel];
    return ctx.renderer ? ctx.renderer->lastFrameMetadata() : QnMetaDataV1Ptr();
}

Qn::RenderStatus QnResourceWidgetRenderer::paint(int channel, const QRectF &rect, qreal opacity) {
    frameDisplayed();

    RenderingTools& ctx = m_channelRenderers[channel];
    if( !ctx.renderer )
        return Qn::NothingRendered;
    ctx.uploader->setOpacity(opacity);
    return ctx.renderer->paint(rect);
}

void QnResourceWidgetRenderer::draw(const QSharedPointer<CLVideoDecoderOutput>& image) {
    RenderingTools& ctx = m_channelRenderers[image->channel];
    if( !ctx.uploader )
        return;
    ctx.uploader->uploadDecodedPicture( image );

    QSize sourceSize = QSize(image->width * image->sample_aspect_ratio, image->height);
    
    if(m_sourceSize != sourceSize) {
        m_sourceSize = sourceSize;
        emit sourceSizeChanged(sourceSize);
    }
}

void QnResourceWidgetRenderer::waitForFrameDisplayed(int /*channel*/) {
    //RenderingTools& ctx = m_channelRenderers[channel];
    //if( !ctx.uploader )
    //    return;
    //ctx.uploader->waitForCurrentDecodedPictureRendered();
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

QSize QnResourceWidgetRenderer::sourceSize() const {
    QMutexLocker locker(&m_mutex);

    return m_sourceSize;
}

const QGLContext* QnResourceWidgetRenderer::glContext() const
{
    return m_glContext;
}
