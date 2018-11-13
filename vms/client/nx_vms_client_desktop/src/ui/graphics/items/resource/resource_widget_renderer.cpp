#include "resource_widget_renderer.h"

#include <camera/gl_renderer.h>
#include <utils/common/warnings.h>
#include <utils/common/performance.h>

#include <client/client_runtime_settings.h>
#include <client/client_settings.h>

#include <nx/streaming/config.h>

#include "decodedpicturetoopengluploader.h"
#include "decodedpicturetoopengluploadercontextpool.h"

using namespace std::chrono;

const microseconds kNoPtsValue{AV_NOPTS_VALUE};

QnResourceWidgetRenderer::QnResourceWidgetRenderer(QObject* parent, QGLContext* context):
    QnAbstractRenderer(parent),
    m_glContext(context)
{
    NX_ASSERT(context);

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_displayRect[i] = QRectF(0, 0, 1, 1);

    const auto currentContextBak = QGLContext::currentContext();
    if (context && currentContextBak != context)
        const_cast<QGLContext*>(context)->makeCurrent();

    if (context && currentContextBak != context)
    {
        if (currentContextBak)
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
    if (!channelCount)
        return;

    m_glContext->makeCurrent();

    for (int i = channelCount; i < m_channelRenderers.count(); ++i)
    {
        delete m_channelRenderers[i].renderer;
        delete m_channelRenderers[i].uploader;
    }

    m_panoFactor = channelCount;

    m_channelRenderers.resize(channelCount);
    m_channelSourceSize.resize(channelCount);
    m_renderingEnabled.fill(true, channelCount);

    for (auto& ctx: m_channelRenderers)
    {
        if (ctx.uploader)
            continue;

        ctx = {};
        ctx.uploader = new DecodedPictureToOpenGLUploader(m_glContext);
        ctx.uploader->setForceSoftYUV(qnRuntime->isSoftwareYuv());
        ctx.renderer = new QnGLRenderer(m_glContext, *ctx.uploader);
        ctx.renderer->setBlurEnabled(qnSettings->isGlBlurEnabled());
        ctx.renderer->setScreenshotInterface(m_screenshotInterface);
        ctx.uploader->setYV12ToRgbShaderUsed(ctx.renderer->isYV12ToRgbShaderUsed());
        ctx.uploader->setNV12ToRgbShaderUsed(ctx.renderer->isNV12ToRgbShaderUsed());
    }
}

void QnResourceWidgetRenderer::destroyAsync()
{
    emit beforeDestroy();
    QnAbstractRenderer::destroyAsync();

    for (const auto& ctx: m_channelRenderers)
    {
        if (ctx.renderer)
            ctx.renderer->beforeDestroy();
        if (ctx.uploader)
            ctx.uploader->beforeDestroy();
    }
}

QnResourceWidgetRenderer::~QnResourceWidgetRenderer()
{
    while (!m_channelRenderers.empty())
    {
        auto ctx = m_channelRenderers.takeLast();
        delete ctx.renderer;
        delete ctx.uploader;
    }
}

void QnResourceWidgetRenderer::pleaseStop()
{
    for (const auto& ctx: m_channelRenderers)
    {
        if (ctx.uploader)
            ctx.uploader->pleaseStop();
    }
}

void QnResourceWidgetRenderer::update()
{
    //renderer->update() is not needed anymore since during paint renderer takes newest decoded picture

    //foreach(RenderingTools ctx, m_channelRenderers)
    //{
    //    if( ctx.renderer )
    //        ctx.renderer->update();
    //}
}

microseconds QnResourceWidgetRenderer::getTimestampOfNextFrameToRender(int channel) const
{
    const auto& ctx = m_channelRenderers[channel];

    if (ctx.timestampBlocked
        || (ctx.framesSinceJump == 0 && ctx.forcedTimestampValue != kNoPtsValue))
    {
        return ctx.forcedTimestampValue;
    }

    if (!ctx.uploader || !ctx.renderer)
        return kNoPtsValue;

    microseconds ts{ctx.uploader->nextFrameToDisplayTimestamp()};
    if (ts == kNoPtsValue)
        ts = microseconds{ctx.renderer->lastDisplayedTime()};

    return ts;
}

void QnResourceWidgetRenderer::blockTimeValue(int channelNumber, microseconds timestamp) const
{
    auto& ctx = m_channelRenderers[channelNumber];
    ctx.timestampBlocked = true;
    ctx.forcedTimestampValue = timestamp;
}

void QnResourceWidgetRenderer::unblockTimeValue(int channelNumber) const
{
    auto& ctx = m_channelRenderers[channelNumber];
    if (!ctx.timestampBlocked)
        return; //< TODO: Is nested blocking needed?

    ctx.timestampBlocked = false;
    ctx.framesSinceJump = 0;
}

bool QnResourceWidgetRenderer::isTimeBlocked(int channelNumber) const
{
    const auto& ctx = m_channelRenderers[channelNumber];
    return ctx.timestampBlocked;
}


bool QnResourceWidgetRenderer::isLowQualityImage(int channel) const
{
    const auto& ctx = m_channelRenderers[channel];
    return ctx.renderer && ctx.renderer->isLowQualityImage();
}

bool QnResourceWidgetRenderer::isHardwareDecoderUsed(int channel) const
{
    const auto& ctx = m_channelRenderers[channel];
    return ctx.renderer && ctx.renderer->isHardwareDecoderUsed();
}

microseconds QnResourceWidgetRenderer::lastDisplayedTimestamp(int channel) const
{
    if (m_channelRenderers.size() <= channel)
        return -1us;

    const auto& ctx = m_channelRenderers[channel];
    return ctx.renderer ? microseconds(ctx.renderer->lastDisplayedTime()) : -1us;
}

void QnResourceWidgetRenderer::setBlurFactor(qreal value)
{
    m_blurFactor = value;
}

Qn::RenderStatus QnResourceWidgetRenderer::paint(
    int channel, const QRectF& sourceRect, const QRectF& targetRect, qreal opacity)
{
    if (m_channelRenderers.size() <= channel)
        return Qn::NothingRendered;

    auto& ctx = m_channelRenderers[channel];
    if (!ctx.renderer)
        return Qn::NothingRendered;

    ctx.uploader->setOpacity(opacity);
    ctx.renderer->setBlurFactor(m_blurFactor);
    return ctx.renderer->paint(sourceRect, targetRect);
}

Qn::RenderStatus QnResourceWidgetRenderer::discardFrame(int channel)
{
    if (m_channelRenderers.size() <= channel)
        return Qn::NothingRendered;

    const auto& ctx = m_channelRenderers[channel];
    if (!ctx.renderer)
        return Qn::NothingRendered;

    return ctx.renderer->discardFrame();
}

void QnResourceWidgetRenderer::skip(int channel)
{
    if (auto& ctx = m_channelRenderers[channel]; ctx.renderer)
        ctx.uploader->discardAllFramesPostedToDisplay();
}

void QnResourceWidgetRenderer::setDisplayedRect(int channel, const QRectF& rect)
{
    if (m_channelRenderers.size() <= channel)
        return;

    m_displayRect[channel] = rect;

    auto& ctx = m_channelRenderers[channel];
    if (ctx.renderer)
        ctx.renderer->setDisplayedRect(rect);
}

void QnResourceWidgetRenderer::setPaused(bool value)
{
    for (const auto& ctx: m_channelRenderers)
        ctx.renderer->setPaused(value);
}

void QnResourceWidgetRenderer::setScreenshotInterface(ScreenshotInterface* value)
{
    m_screenshotInterface = value;
    for (const auto& ctx: m_channelRenderers)
        ctx.renderer->setScreenshotInterface(value);
}

bool QnResourceWidgetRenderer::isEnabled(int channelNumber) const
{
    QnMutexLocker lk(&m_mutex);
    return channelNumber < m_renderingEnabled.size() && m_renderingEnabled[channelNumber];
}

void QnResourceWidgetRenderer::setEnabled(int channel, bool enabled)
{
    if (m_channelRenderers.size() <= channel)
        return;

    auto& ctx = m_channelRenderers[channel];
    if (!ctx.uploader)
        return;

    QnMutexLocker lk(&m_mutex);

    m_renderingEnabled[channel] = enabled;
    if (!enabled)
        ctx.uploader->discardAllFramesPostedToDisplay();
}

void QnResourceWidgetRenderer::draw(const QSharedPointer<CLVideoDecoderOutput>& image)
{
    {
        QnMutexLocker lk(&m_mutex);

        if (!m_renderingEnabled[image->channel])
            return;

        auto& ctx = m_channelRenderers[image->channel];
        if (!ctx.uploader)
            return;

        ctx.uploader->uploadDecodedPicture(image, m_displayRect[image->channel]);
        ++ctx.framesSinceJump;
    }

    const QSize sourceSize((int) (image->width * image->sample_aspect_ratio), image->height);

    if (m_channelSourceSize[image->channel] == sourceSize)
        return;

    m_channelSourceSize[image->channel] = sourceSize;
    m_sourceSize = getMostFrequentChannelSourceSize();

    emit sourceSizeChanged();
}

void QnResourceWidgetRenderer::discardAllFramesPostedToDisplay(int channel)
{
    if (auto& ctx = m_channelRenderers[channel]; ctx.uploader)
    {
        ctx.uploader->discardAllFramesPostedToDisplay();
        ctx.uploader->waitForCurrentFrameDisplayed();
    }
}

void QnResourceWidgetRenderer::waitForFrameDisplayed(int channel)
{
    if (auto& ctx = m_channelRenderers[channel]; ctx.uploader)
        ctx.uploader->ensureAllFramesWillBeDisplayed();
}

void QnResourceWidgetRenderer::waitForQueueLessThan(int channel, int maxSize)
{
    if (auto& ctx = m_channelRenderers[channel]; ctx.uploader)
        ctx.uploader->ensureQueueLessThen(maxSize);
}

void QnResourceWidgetRenderer::finishPostedFramesRender(int channel)
{
    if (auto& ctx = m_channelRenderers[channel]; ctx.uploader)
        ctx.uploader->waitForAllFramesDisplayed();
}

QSize QnResourceWidgetRenderer::sizeOnScreen(int /*channel*/) const
{
    QnMutexLocker locker(&m_mutex);
    return m_channelScreenSize;
}

void QnResourceWidgetRenderer::setChannelScreenSize(const QSize& screenSize)
{
    QnMutexLocker locker(&m_mutex);
    m_channelScreenSize = screenSize;
}

bool QnResourceWidgetRenderer::constantDownscaleFactor() const
{
    return !m_channelRenderers.empty() && m_channelRenderers[0].renderer
        && m_channelRenderers[0].renderer->isFisheyeEnabled();
}

QSize QnResourceWidgetRenderer::sourceSize() const
{
    QnMutexLocker locker(&m_mutex);
    return m_sourceSize;
}

const QGLContext* QnResourceWidgetRenderer::glContext() const
{
    return m_glContext;
}

bool QnResourceWidgetRenderer::isDisplaying(
    const QSharedPointer<CLVideoDecoderOutput>& image) const
{
    const auto& ctx = m_channelRenderers[image->channel];
    return ctx.uploader && ctx.uploader->isUsingFrame(image);
}

void QnResourceWidgetRenderer::setImageCorrection(const ImageCorrectionParams& params)
{
    for (auto& ctx: m_channelRenderers)
    {
        if (!ctx.uploader)
            continue;

        ctx.uploader->setImageCorrection(params);
        if (ctx.renderer)
            ctx.renderer->setImageCorrectionParams(params);
    }
}

void QnResourceWidgetRenderer::setFisheyeController(QnFisheyePtzController* controller)
{
    for (auto& ctx: m_channelRenderers)
    {
        if (!ctx.uploader)
            continue;

        if (ctx.renderer)
            ctx.renderer->setFisheyeController(controller);
    }
}

void QnResourceWidgetRenderer::setHistogramConsumer(QnHistogramConsumer* value)
{
    for (auto& ctx: m_channelRenderers)
    {
        if (ctx.renderer)
            ctx.renderer->setHistogramConsumer(value);
    }
}

QSize QnResourceWidgetRenderer::getMostFrequentChannelSourceSize() const
{
    const auto channelCount = m_channelSourceSize.size();

    QHash<int, int> widthFrequencyMap;
    QHash<int, int> heightFrequentMap;

    for (int channel = 0; channel < channelCount; ++channel)
    {
        if (!m_channelSourceSize[channel].isEmpty())
        {
            ++widthFrequencyMap[m_channelSourceSize[channel].width()];
            ++heightFrequentMap[m_channelSourceSize[channel].height()];
        }
    }

    const auto getMostFrequentSize =
        [](const QHash<int, int>& sizeMap)
        {
            auto maxSize = std::max_element(sizeMap.begin(), sizeMap.end(),
                [](const int l, const int r) { return l < r; });

            return maxSize == sizeMap.end() ? -1 : maxSize.key();
        };

    return QSize(
        getMostFrequentSize(widthFrequencyMap),
        getMostFrequentSize(heightFrequentMap));
}
