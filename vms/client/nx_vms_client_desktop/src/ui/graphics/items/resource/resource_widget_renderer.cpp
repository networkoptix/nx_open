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
}

void QnResourceWidgetRenderer::setChannelCount(int channelCount)
{
    if (channelCount < 1)
        return;

    m_glContext->makeCurrent();
    m_panoFactor = channelCount;
    m_renderingContexts.resize(channelCount);

    for (auto& ctx: m_renderingContexts)
    {
        if (ctx.uploader)
            continue;

        ctx = {};
        ctx.uploader = std::make_shared<DecodedPictureToOpenGLUploader>(m_glContext);
        ctx.uploader->setForceSoftYUV(qnRuntime->isSoftwareYuv());
        ctx.renderer = std::make_shared<QnGLRenderer>(m_glContext, *ctx.uploader);
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

    for (const auto& ctx: m_renderingContexts)
    {
        if (ctx.renderer)
            ctx.renderer->beforeDestroy();
        if (ctx.uploader)
            ctx.uploader->beforeDestroy();
    }
}

QnResourceWidgetRenderer::~QnResourceWidgetRenderer()
{
}

void QnResourceWidgetRenderer::pleaseStop()
{
    for (const auto& ctx: m_renderingContexts)
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
    const auto& ctx = m_renderingContexts[channel];

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

void QnResourceWidgetRenderer::blockTimeValue(int channel, microseconds timestamp)
{
    auto& ctx = m_renderingContexts[channel];
    ctx.timestampBlocked = true;
    ctx.forcedTimestampValue = timestamp;
}

void QnResourceWidgetRenderer::unblockTimeValue(int channel)
{
    auto& ctx = m_renderingContexts[channel];
    if (!ctx.timestampBlocked)
        return; //< TODO: Is nested blocking needed?

    ctx.timestampBlocked = false;
    ctx.framesSinceJump = 0;
}

bool QnResourceWidgetRenderer::isTimeBlocked(int channel) const
{
    const auto& ctx = m_renderingContexts[channel];
    return ctx.timestampBlocked;
}


bool QnResourceWidgetRenderer::isLowQualityImage(int channel) const
{
    const auto& ctx = m_renderingContexts[channel];
    return ctx.renderer && ctx.renderer->isLowQualityImage();
}

bool QnResourceWidgetRenderer::isHardwareDecoderUsed(int channel) const
{
    const auto& ctx = m_renderingContexts[channel];
    return ctx.renderer && ctx.renderer->isHardwareDecoderUsed();
}

microseconds QnResourceWidgetRenderer::lastDisplayedTimestamp(int channel) const
{
    if (m_renderingContexts.size() <= channel)
        return -1us;

    const auto& ctx = m_renderingContexts[channel];
    return ctx.renderer ? microseconds(ctx.renderer->lastDisplayedTime()) : -1us;
}

void QnResourceWidgetRenderer::setBlurFactor(qreal value)
{
    m_blurFactor = value;
}

Qn::RenderStatus QnResourceWidgetRenderer::paint(
    int channel, const QRectF& sourceRect, const QRectF& targetRect, qreal opacity)
{
    if (m_renderingContexts.size() <= channel)
        return Qn::NothingRendered;

    auto& ctx = m_renderingContexts[channel];
    if (!ctx.renderer)
        return Qn::NothingRendered;

    ctx.uploader->setOpacity(opacity);
    ctx.renderer->setBlurFactor(m_blurFactor);
    return ctx.renderer->paint(sourceRect, targetRect);
}

Qn::RenderStatus QnResourceWidgetRenderer::discardFrame(int channel)
{
    if (m_renderingContexts.size() <= channel)
        return Qn::NothingRendered;

    const auto& ctx = m_renderingContexts[channel];
    if (!ctx.renderer)
        return Qn::NothingRendered;

    return ctx.renderer->discardFrame();
}

void QnResourceWidgetRenderer::skip(int channel)
{
    if (auto& ctx = m_renderingContexts[channel]; ctx.renderer)
        ctx.uploader->discardAllFramesPostedToDisplay();
}

void QnResourceWidgetRenderer::setDisplayedRect(int channel, const QRectF& rect)
{
    if (m_renderingContexts.size() <= channel)
        return;

    auto& ctx = m_renderingContexts[channel];
    ctx.displayRect = rect;
    if (ctx.renderer)
        ctx.renderer->setDisplayedRect(rect);
}

void QnResourceWidgetRenderer::setPaused(bool value)
{
    for (const auto& ctx: m_renderingContexts)
        ctx.renderer->setPaused(value);
}

void QnResourceWidgetRenderer::setScreenshotInterface(ScreenshotInterface* value)
{
    m_screenshotInterface = value;
    for (const auto& ctx: m_renderingContexts)
        ctx.renderer->setScreenshotInterface(value);
}

bool QnResourceWidgetRenderer::isEnabled(int channel) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return channel < m_renderingContexts.size() && m_renderingContexts[channel].renderingEnabled;
}

void QnResourceWidgetRenderer::setEnabled(int channel, bool enabled)
{
    if (m_renderingContexts.size() <= channel)
        return;

    auto& ctx = m_renderingContexts[channel];
    if (!ctx.uploader)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);

    ctx.renderingEnabled = enabled;
    if (!enabled)
        ctx.uploader->discardAllFramesPostedToDisplay();
}

void QnResourceWidgetRenderer::draw(const QSharedPointer<CLVideoDecoderOutput>& image)
{
    auto& ctx = m_renderingContexts[image->channel];
    if (!ctx.uploader)
        return;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (!ctx.renderingEnabled)
            return;

        ctx.uploader->uploadDecodedPicture(image, ctx.displayRect);
        ++ctx.framesSinceJump;
    }

    const QSize sourceSize((int) (image->width * image->sample_aspect_ratio), image->height);

    if (ctx.channelSourceSize == sourceSize)
        return;

    ctx.channelSourceSize = sourceSize;
    m_sourceSize = getMostFrequentChannelSourceSize();

    emit sourceSizeChanged();
}

void QnResourceWidgetRenderer::discardAllFramesPostedToDisplay(int channel)
{
    if (auto& ctx = m_renderingContexts[channel]; ctx.uploader)
    {
        ctx.uploader->discardAllFramesPostedToDisplay();
        ctx.uploader->waitForCurrentFrameDisplayed();
    }
}

void QnResourceWidgetRenderer::waitForFrameDisplayed(int channel)
{
    if (auto& ctx = m_renderingContexts[channel]; ctx.uploader)
        ctx.uploader->ensureAllFramesWillBeDisplayed();
}

void QnResourceWidgetRenderer::waitForQueueLessThan(int channel, int maxSize)
{
    if (auto& ctx = m_renderingContexts[channel]; ctx.uploader)
        ctx.uploader->ensureQueueLessThen(maxSize);
}

void QnResourceWidgetRenderer::finishPostedFramesRender(int channel)
{
    if (auto& ctx = m_renderingContexts[channel]; ctx.uploader)
        ctx.uploader->waitForAllFramesDisplayed();
}

QSize QnResourceWidgetRenderer::sizeOnScreen(int /*channel*/) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_channelScreenSize;
}

void QnResourceWidgetRenderer::setChannelScreenSize(const QSize& screenSize)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_channelScreenSize = screenSize;
}

bool QnResourceWidgetRenderer::constantDownscaleFactor() const
{
    return !m_renderingContexts.empty() && m_renderingContexts[0].renderer
        && m_renderingContexts[0].renderer->isFisheyeEnabled();
}

QSize QnResourceWidgetRenderer::sourceSize() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_sourceSize;
}

const QGLContext* QnResourceWidgetRenderer::glContext() const
{
    return m_glContext;
}

bool QnResourceWidgetRenderer::isDisplaying(
    const QSharedPointer<CLVideoDecoderOutput>& image) const
{
    const auto& ctx = m_renderingContexts[image->channel];
    return ctx.uploader && ctx.uploader->isUsingFrame(image);
}

void QnResourceWidgetRenderer::setImageCorrection(const nx::vms::api::ImageCorrectionData& params)
{
    for (auto& ctx: m_renderingContexts)
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
    for (auto& ctx: m_renderingContexts)
    {
        if (!ctx.uploader)
            continue;

        if (ctx.renderer)
            ctx.renderer->setFisheyeController(controller);
    }
}

void QnResourceWidgetRenderer::setHistogramConsumer(QnHistogramConsumer* value)
{
    for (auto& ctx: m_renderingContexts)
    {
        if (ctx.renderer)
            ctx.renderer->setHistogramConsumer(value);
    }
}

QSize QnResourceWidgetRenderer::getMostFrequentChannelSourceSize() const
{
    QHash<int, int> widthFrequencyMap;
    QHash<int, int> heightFrequencyMap;

    for (auto& ctx: m_renderingContexts)
    {
        if (!ctx.channelSourceSize.isEmpty())
        {
            ++widthFrequencyMap[ctx.channelSourceSize.width()];
            ++heightFrequencyMap[ctx.channelSourceSize.height()];
        }
    }

    const auto getMostFrequentSize =
        [](const QHash<int, int>& sizeMap)
        {
            auto maxSizeIt = std::max_element(sizeMap.begin(), sizeMap.end(),
                [](const int l, const int r) { return l < r; });

            return maxSizeIt == sizeMap.end() ? -1 : maxSizeIt.key();
        };

    return QSize(
        getMostFrequentSize(widthFrequencyMap),
        getMostFrequentSize(heightFrequencyMap));
}
