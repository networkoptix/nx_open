
#include "resource_widget_renderer.h"

#include <QtCore/QMutexLocker>

#include <camera/gl_renderer.h>
#include <utils/common/warnings.h>
#include <utils/common/performance.h>
#include <client/client_runtime_settings.h>

#include "decodedpicturetoopengluploader.h"
#include "decodedpicturetoopengluploadercontextpool.h"

QnResourceWidgetRenderer::QnResourceWidgetRenderer(QObject* parent, QGLContext* context )
:
    QnAbstractRenderer( parent ),
    m_glContext( context ),
    m_screenshotInterface(0),
    m_panoFactor(1)
{
    Q_ASSERT( context != NULL );

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_displayRect[i] = QRectF(0, 0, 1, 1);

    const QGLContext* currentContextBak = QGLContext::currentContext();
    if( context && (currentContextBak != context) )
        const_cast<QGLContext*>(context)->makeCurrent();

    if( context && currentContextBak != context ) {
        if( currentContextBak )
            const_cast<QGLContext*>(currentContextBak)->makeCurrent();
        else
            const_cast<QGLContext*>(context)->doneCurrent();
    }

    setChannelCount(1);

    connect(this, &QnAbstractRenderer::canBeDestroyed, this, &QObject::deleteLater);

#ifdef TEST_FISHEYE_CALIBRATOR
    m_isCircleDetected = false;
#endif
}

void QnResourceWidgetRenderer::setChannelCount(int channelCount)
{
    if( !channelCount )
        return;

    Q_ASSERT( m_glContext != NULL );
    m_glContext->makeCurrent();

    for (int i = channelCount; (uint)i < m_channelRenderers.size(); ++i)
    {
        delete m_channelRenderers[i].renderer;
        delete m_channelRenderers[i].uploader;
    }
    m_panoFactor = channelCount;

    m_channelRenderers.resize( channelCount );
    m_renderingEnabled.resize( channelCount, true );

    for( int i = 0; i < channelCount; ++i )
    {
        if (m_channelRenderers[i].uploader == 0) {
            RenderingTools renderingTools;
            renderingTools.uploader = new DecodedPictureToOpenGLUploader( m_glContext );
            renderingTools.uploader->setForceSoftYUV( qnRuntime->isSoftwareYuv() );
            renderingTools.renderer = new QnGLRenderer( m_glContext, *renderingTools.uploader );
            renderingTools.renderer->setScreenshotInterface(m_screenshotInterface);
            renderingTools.uploader->setYV12ToRgbShaderUsed(renderingTools.renderer->isYV12ToRgbShaderUsed());
            renderingTools.uploader->setNV12ToRgbShaderUsed(renderingTools.renderer->isNV12ToRgbShaderUsed());
            m_channelRenderers[i] = renderingTools;
        }
    }
}

void QnResourceWidgetRenderer::destroyAsync()
{
    emit beforeDestroy();
    QnAbstractRenderer::destroyAsync();
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

void QnResourceWidgetRenderer::pleaseStop()
{
    foreach(RenderingTools ctx, m_channelRenderers)
    {
        if( ctx.uploader )
            ctx.uploader->pleaseStop();
    }
}

void QnResourceWidgetRenderer::update() {
    //renderer->update() is not needed anymore since during paint renderer takes newest decoded picture

    //foreach(RenderingTools ctx, m_channelRenderers)
    //{
    //    if( ctx.renderer )
    //        ctx.renderer->update();
    //}
}

qint64 QnResourceWidgetRenderer::getTimestampOfNextFrameToRender(int channel) const {
    const RenderingTools& ctx = m_channelRenderers[channel];
    //return ctx.renderer ? ctx.renderer->lastDisplayedTime() : AV_NOPTS_VALUE;

    if( ctx.timestampBlocked || (ctx.framesSinceJump == 0 && ctx.forcedTimestampValue != (qint64)AV_NOPTS_VALUE) )
        return ctx.forcedTimestampValue;

    if( !ctx.uploader || !ctx.renderer )
        return AV_NOPTS_VALUE;
    qint64 ts = ctx.uploader->nextFrameToDisplayTimestamp();
    if( ts == (qint64)AV_NOPTS_VALUE )
        ts = ctx.renderer->lastDisplayedTime();

    return ts;
}

void QnResourceWidgetRenderer::blockTimeValue(int channelNumber, qint64  timestamp ) const
{
    RenderingTools& ctx = m_channelRenderers[channelNumber];
    //if (ctx.renderer)
    //    ctx.renderer->blockTimeValue(timestamp);

    ctx.timestampBlocked = true;
    ctx.forcedTimestampValue = timestamp;
}

void QnResourceWidgetRenderer::unblockTimeValue(int channelNumber) const
{
    RenderingTools& ctx = m_channelRenderers[channelNumber];
    if( !ctx.timestampBlocked )
        return; //TODO/IMPL is nested blocking needed?
    ctx.timestampBlocked = false;
    ctx.framesSinceJump = 0;
}

bool QnResourceWidgetRenderer::isTimeBlocked(int channelNumber) const
{
    const RenderingTools& ctx = m_channelRenderers[channelNumber];
    return ctx.timestampBlocked;
}


qint64 QnResourceWidgetRenderer::isLowQualityImage(int channel) const {
    const RenderingTools& ctx = m_channelRenderers[channel];
    return ctx.renderer ? ctx.renderer->isLowQualityImage() : 0;
}

bool QnResourceWidgetRenderer::isHardwareDecoderUsed(int channel) const
{
    const RenderingTools& ctx = m_channelRenderers[channel];
    return ctx.renderer ? ctx.renderer->isHardwareDecoderUsed() : 0;
}

QnMetaDataV1Ptr QnResourceWidgetRenderer::lastFrameMetadata(int channel) const
{
    const RenderingTools& ctx = m_channelRenderers[channel];
    return ctx.renderer ? ctx.renderer->lastFrameMetadata() : QnMetaDataV1Ptr();
}

Qn::RenderStatus QnResourceWidgetRenderer::paint(int channel, const QRectF &sourceRect, const QRectF &targetRect, qreal opacity) {
    if (m_channelRenderers.size() < static_cast<size_t>(channel))
        return Qn::NothingRendered;
    RenderingTools &ctx = m_channelRenderers[channel];
    if(!ctx.renderer)
        return Qn::NothingRendered;
    ctx.uploader->setOpacity(opacity);
    return ctx.renderer->paint(sourceRect, targetRect);
}

void QnResourceWidgetRenderer::skip(int channel) {
    RenderingTools &ctx = m_channelRenderers[channel];
    if(!ctx.renderer)
        return;
    ctx.uploader->discardAllFramesPostedToDisplay();
}

void QnResourceWidgetRenderer::setDisplayedRect(int channel, const QRectF& rect)
{
    m_displayRect[channel] = rect;

    RenderingTools& ctx = m_channelRenderers[channel];
    if( ctx.renderer)
        ctx.renderer->setDisplayedRect(rect);
}

void QnResourceWidgetRenderer::setPaused(bool value)
{
    for (uint i = 0; i < m_channelRenderers.size(); ++i)
        m_channelRenderers[i].renderer->setPaused(value);
}

void QnResourceWidgetRenderer::setScreenshotInterface(ScreenshotInterface* value)
{
    m_screenshotInterface = value;
    for (uint i = 0; i < m_channelRenderers.size(); ++i)
        m_channelRenderers[i].renderer->setScreenshotInterface(value);
}

bool QnResourceWidgetRenderer::isEnabled(int channelNumber) const
{
    QnMutexLocker lk( &m_mutex );
    return m_renderingEnabled[channelNumber];
}

void QnResourceWidgetRenderer::setEnabled(int channelNumber, bool enabled)
{
    RenderingTools& ctx = m_channelRenderers[channelNumber];
    if( !ctx.uploader )
        return;

    QnMutexLocker lk( &m_mutex );

    m_renderingEnabled[channelNumber] = enabled;
    if( !enabled )
        ctx.uploader->discardAllFramesPostedToDisplay();
}

void QnResourceWidgetRenderer::draw(const QSharedPointer<CLVideoDecoderOutput>& image)
{
    {
        QnMutexLocker lk( &m_mutex );

        if( !m_renderingEnabled[image->channel] )
            return;

        RenderingTools& ctx = m_channelRenderers[image->channel];
        if( !ctx.uploader )
            return;

        ctx.uploader->uploadDecodedPicture( image, m_displayRect[image->channel]);
        ++ctx.framesSinceJump;
    }

    QSize sourceSize = QSize(image->width * image->sample_aspect_ratio, image->height);
    if(m_sourceSize == sourceSize)
        return;

    m_sourceSize = sourceSize;

    emit sourceSizeChanged();
}

void QnResourceWidgetRenderer::discardAllFramesPostedToDisplay(int channel)
{
    RenderingTools& ctx = m_channelRenderers[channel];
    if( !ctx.uploader )
        return;
    ctx.uploader->discardAllFramesPostedToDisplay();
    ctx.uploader->waitForCurrentFrameDisplayed();
}

void QnResourceWidgetRenderer::waitForFrameDisplayed(int channel) {
    RenderingTools& ctx = m_channelRenderers[channel];
    if( !ctx.uploader )
        return;
    ctx.uploader->ensureAllFramesWillBeDisplayed();
}

void QnResourceWidgetRenderer::waitForQueueLessThan(int channel, int maxSize) {
    RenderingTools& ctx = m_channelRenderers[channel];
    if( !ctx.uploader )
        return;
    ctx.uploader->ensureQueueLessThen(maxSize);
}

void QnResourceWidgetRenderer::finishPostedFramesRender(int channel)
{
    RenderingTools& ctx = m_channelRenderers[channel];
    if( !ctx.uploader )
        return;
    ctx.uploader->waitForAllFramesDisplayed();
}

QSize QnResourceWidgetRenderer::sizeOnScreen(unsigned int /*channel*/) const {
    QnMutexLocker locker( &m_mutex );

    return m_channelScreenSize;
}

void QnResourceWidgetRenderer::setChannelScreenSize(const QSize &screenSize) {
    QnMutexLocker locker( &m_mutex );

    m_channelScreenSize = screenSize;
}

bool QnResourceWidgetRenderer::constantDownscaleFactor() const {
    return !m_channelRenderers.empty() && m_channelRenderers[0].renderer && m_channelRenderers[0].renderer->isFisheyeEnabled();
}

QSize QnResourceWidgetRenderer::sourceSize() const {
    QnMutexLocker locker( &m_mutex );

    //return QSize(m_sourceSize.width() * m_panoFactor, m_sourceSize.height());
    return QSize(m_sourceSize.width(), m_sourceSize.height());
}

const QGLContext* QnResourceWidgetRenderer::glContext() const
{
    return m_glContext;
}

bool QnResourceWidgetRenderer::isDisplaying( const QSharedPointer<CLVideoDecoderOutput>& image ) const
{
    RenderingTools& ctx = m_channelRenderers[image->channel];
    if( !ctx.uploader )
        return false;
    return ctx.uploader->isUsingFrame( image );
}

void QnResourceWidgetRenderer::setImageCorrection(const ImageCorrectionParams& params)
{
    for (uint i = 0; i < m_channelRenderers.size(); ++i){
        RenderingTools& ctx = m_channelRenderers[i];
        if( !ctx.uploader )
            continue;
        ctx.uploader->setImageCorrection(params);
        ctx.renderer->setImageCorrectionParams(params);
    }
}

void QnResourceWidgetRenderer::setFisheyeController(QnFisheyePtzController* controller)
{
    for (uint i = 0; i < m_channelRenderers.size(); ++i){
        RenderingTools& ctx = m_channelRenderers[i];
        if( !ctx.uploader )
            continue;
        ctx.renderer->setFisheyeController(controller);
    }
}

void QnResourceWidgetRenderer::setHistogramConsumer(QnHistogramConsumer* value)
{
    for (uint i = 0; i < m_channelRenderers.size(); ++i)
    {
        RenderingTools& ctx = m_channelRenderers[i];
        if( ctx.renderer )
            ctx.renderer->setHistogramConsumer(value);
    }
}
