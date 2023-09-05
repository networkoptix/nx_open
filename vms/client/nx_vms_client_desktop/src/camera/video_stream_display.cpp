// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_stream_display.h"

#include <algorithm>
#include <chrono>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <client/client_module.h>
#include <core/resource/resource_property_key.h>
#include <core/resource/security_cam_resource.h>
#include <decoders/video/abstract_video_decoder.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/media/quick_sync/qsv_supported.h>
#include <nx/media/utils.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <utils/common/adaptive_sleep.h>

#ifdef __QSV_SUPPORTED__
#include <nx/media/nvidia/nvidia_video_decoder_old_player.h>
#include <nx/media/quick_sync/quick_sync_video_decoder_old_player.h>
#endif // __QSV_SUPPORTED__

#include "buffered_frame_displayer.h"
#include "gl_renderer.h"

using namespace std::chrono;
using namespace nx::vms::client::desktop;

static const double FPS_EPS = 1e-6;

namespace {

constexpr int kMaxFrameQueueSize = 6;
constexpr int kMaxReverseQueueSize = 1024 * 1024 * 300; //< In bytes.

bool isNvidia()
{
    return QnGlFunctions::openGLInfo().vendor.toLower().contains("nvidia");
}

bool isIntel()
{
    return QnGlFunctions::openGLInfo().vendor.toLower().contains("intel");
}

QnFrameScaler::DownscaleFactor findScaleFactor(int width, int height, int fitWidth, int fitHeight)
{
    if (fitWidth * 8 <= width && fitHeight * 8 <= height)
        return QnFrameScaler::factor_8;
    if (fitWidth * 4 <= width && fitHeight * 4 <= height)
        return QnFrameScaler::factor_4;
    if (fitWidth * 2 <= width && fitHeight * 2 <= height)
        return QnFrameScaler::factor_2;

    return QnFrameScaler::factor_1;
}

}

QnVideoStreamDisplay::QnVideoStreamDisplay(
    const QnMediaResourcePtr& resource, bool canDownscale, int channelNumber):
    m_resource(resource),
    m_decodeMode(QnAbstractVideoDecoder::DecodeMode_Full),
    m_canDownscale(canDownscale),
    m_channelNumber(channelNumber),
    m_prevFactor(QnFrameScaler::factor_1),
    m_scaleFactor(QnFrameScaler::factor_1),
    m_previousOnScreenSize(0, 0),
    m_scaleContext(nullptr),
    m_outputWidth(0),
    m_outputHeight(0),
    m_reverseMode(false),
    m_prevReverseMode(false),
    m_flushedBeforeReverseStart(false),
    m_reverseSizeInBytes(0),
    m_timeChangeEnabled(true),
    m_canUseBufferedFrameDisplayer(true),
    m_rawDataSize(0,0),
    m_speed(1.0),
    m_queueWasFilled(false),
    m_needResetDecoder(false),
    m_lastDisplayedFrame(nullptr),
    m_prevSrcWidth(0),
    m_prevSrcHeight(0),
    m_lastIgnoreTime(AV_NOPTS_VALUE),
    m_isPaused(false)
{
}

QnVideoStreamDisplay::~QnVideoStreamDisplay()
{
    for (auto& render: m_renderList)
        render->notInUse();

    freeScaleContext();
}

void QnVideoStreamDisplay::endOfRun()
{
    NX_MUTEX_LOCKER lock(&m_mtx);
    m_decoderData.decoder.reset();
}

int QnVideoStreamDisplay::addRenderer(QnResourceWidgetRenderer* renderer)
{
    {
        NX_MUTEX_LOCKER lock(&m_lastDisplayedFrameMutex);
        renderer->setPaused(m_isPaused);
        if (m_lastDisplayedFrame)
            renderer->draw(m_lastDisplayedFrame, getMaxScreenSizeUnsafe());
    }

    NX_MUTEX_LOCKER lock( &m_renderListMtx );
    renderer->setScreenshotInterface(this);
    m_newList.insert(renderer);
    m_renderListModified = true;
    return m_newList.size();
}

int QnVideoStreamDisplay::removeRenderer(QnResourceWidgetRenderer* renderer)
{
    NX_MUTEX_LOCKER lock( &m_renderListMtx );
    m_newList.erase(renderer);
    m_renderListModified = true;
    return m_newList.size();
}

QnFrameScaler::DownscaleFactor QnVideoStreamDisplay::getCurrentDownscaleFactor() const
{
    return m_scaleFactor;
}

bool QnVideoStreamDisplay::allocScaleContext(
    const CLVideoDecoderOutput& outFrame,
    int newWidth,
    int newHeight)
{
    m_outputWidth = newWidth;
    m_outputHeight = newHeight;
    m_scaleContext = sws_getContext(outFrame.width, outFrame.height, (AVPixelFormat) outFrame.format,
                                    m_outputWidth, m_outputHeight, AV_PIX_FMT_RGBA,
                                    SWS_POINT, nullptr, nullptr, nullptr);
    if (!m_scaleContext)
        NX_ERROR(this, "Can't get swscale context");
    return m_scaleContext != 0;
}

void QnVideoStreamDisplay::freeScaleContext()
{
    if (m_scaleContext)
    {
        sws_freeContext(m_scaleContext);
        m_scaleContext = 0;
    }
}

QnFrameScaler::DownscaleFactor QnVideoStreamDisplay::determineScaleFactor(
    std::set<QnResourceWidgetRenderer*> renderList,
    int channelNumber,
    int srcWidth,
    int srcHeight,
    QnFrameScaler::DownscaleFactor forceScaleFactor)
{
    for (QnResourceWidgetRenderer* render: renderList) {
        if (render->constantDownscaleFactor())
            forceScaleFactor = QnFrameScaler::factor_1;
    }

    if (forceScaleFactor==QnFrameScaler::factor_any) // if nobody pushing lets peek it
    {
        QSize on_screen;
        for (QnResourceWidgetRenderer* render: renderList) {
            QSize size = render->sizeOnScreen(channelNumber);
            on_screen.setWidth(qMax(on_screen.width(), size.width()));
            on_screen.setHeight(qMax(on_screen.height(), size.height()));
        }
        /* Check if item size is not calculated yet. */
        if (on_screen.isEmpty()) {
            on_screen.setHeight(srcHeight);
            on_screen.setWidth(srcWidth);
        }

        m_scaleFactor = findScaleFactor(srcWidth, srcHeight, on_screen.width(), on_screen.height());

        if (m_scaleFactor < m_prevFactor && m_prevSrcWidth == srcWidth && m_prevSrcHeight == srcHeight)
        {
            // new factor is less than prev one; about to change factor => about to increase resource usage
            if ( qAbs((qreal)on_screen.width() - m_previousOnScreenSize.width())/on_screen.width() < 0.05 &&
                qAbs((qreal)on_screen.height() - m_previousOnScreenSize.height())/on_screen.height() < 0.05)
            {
                m_scaleFactor = m_prevFactor; // hold bigger factor ( smaller image )
            }

            // why?
            // we need to do so ( introduce some hysteresis )coz downscaling changes resolution not proportionally some time( cut vertical size a bit )
            // so it may be a loop downscale => changed aspect ratio => upscale => changed aspect ratio => downscale.
        }
        m_prevSrcWidth = srcWidth;
        m_prevSrcHeight = srcHeight;

        if (m_scaleFactor != m_prevFactor)
        {
            m_previousOnScreenSize = on_screen;
            m_prevFactor = m_scaleFactor;
        }
    }
    else
        m_scaleFactor = forceScaleFactor;

    QnFrameScaler::DownscaleFactor rez = m_canDownscale ? qMax(m_scaleFactor, QnFrameScaler::factor_1) : QnFrameScaler::factor_1;
    // If there is no scaling needed check if size is greater than maximum allowed image size (maximum texture size for opengl).
    int newWidth = srcWidth / rez;
    int newHeight = srcHeight / rez;
    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    while ((maxTextureSize > 0 && newWidth > maxTextureSize) || newHeight > maxTextureSize)
    {
        rez = QnFrameScaler::DownscaleFactor ((int)rez * 2);
        newWidth /= 2;
        newHeight /= 2;
    }
    return rez;
}

void QnVideoStreamDisplay::reorderPrevFrames()
{
    if (m_reverseQueue.isEmpty())
        return;
    // find latest GOP (or several GOP. Decoder can add 1-3 (or even more) gops at once as 1 reverse block)
    int i = m_reverseQueue.size()-1;
    for (; i >= 0; --i)
    {
        if (m_reverseQueue[i]->flags & QnAbstractMediaData::MediaFlags_ReverseReordered)
            break;
        if ((m_reverseQueue[i]->flags & QnAbstractMediaData::MediaFlags_ReverseBlockStart) || i == 0)
        {
            std::reverse(m_reverseQueue.begin() + i, m_reverseQueue.end());
            for (int j = i; j < m_reverseQueue.size(); ++j)
                m_reverseQueue[j]->flags |= QnAbstractMediaData::MediaFlags_ReverseReordered;
            return;
        }
    }
    i++;
    if (i < m_reverseQueue.size()) {
        std::reverse(m_reverseQueue.begin() + i, m_reverseQueue.end());
        for (int j = i; j < m_reverseQueue.size(); ++j)
            m_reverseQueue[j]->flags |= QnAbstractMediaData::MediaFlags_ReverseReordered;
    }
}

void QnVideoStreamDisplay::checkQueueOverflow()
{
    if (m_reverseQueue.size() == 1)
        return;

    while (m_reverseSizeInBytes > kMaxReverseQueueSize)
    {
        // drop some frame at queue. Find max interval contains non-dropped frames (and drop frame from mid of this interval)
        int maxInterval = -1;
        //int prevIndex = -1;
        int maxStart = 0;

        for (int i = 0; i < m_reverseQueue.size(); ++i)
        {
            if (m_reverseQueue[i]->data[0])
            {
                int start = i;
                for (; i < m_reverseQueue.size() && m_reverseQueue[i]->data[0]; ++i);

                if (i - start > maxInterval) {
                    maxInterval = i - start;
                    maxStart = start;
                }
            }
        }
        int index;
        if (maxInterval == 1)
        {
            // every 2nd frame already dropped. Change strategy. Increase min hole interval by 1
            int minHole = INT_MAX;
            for (int i = m_reverseQueue.size()-1; i >= 0; --i) {
                if (!m_reverseQueue[i]->data[0])
                {
                    int start = i;
                    for (; i >= 0 && !m_reverseQueue[i]->data[0]; --i);

                    if (start - i < minHole) {
                        minHole = start - i;
                        maxStart = i+1;
                    }
                }
            }
            if (maxStart + minHole < m_reverseQueue.size())
                index = maxStart + minHole; // take right frame from the hole
            else
                index = std::min(maxStart - 1, 0); // take left frame
        }
        else {
            index = maxStart + maxInterval/2;
        }
        if (!NX_ASSERT(m_reverseQueue[index]->data[0]))
            return;
        m_reverseSizeInBytes -= m_reverseQueue[index]->sizeBytes();
        m_reverseQueue[index]->clean();
    }
}

void QnVideoStreamDisplay::waitForFramesDisplayed()
{
    if (m_bufferedFrameDisplayer)
    {
        m_bufferedFrameDisplayer->waitForFramesDisplayed();
    }
    else
    {
        for (const auto& render: m_renderList)
            render->waitForFrameDisplayed(0); //< Wait for the old frame.
    }
    m_queueWasFilled = false;
}

qint64 QnVideoStreamDisplay::nextReverseTime() const
{
    for (int i = 0; i < m_reverseQueue.size(); ++i)
    {
        if (m_reverseQueue[i]->data[0] && m_reverseQueue[i]->pkt_dts != AV_NOPTS_VALUE)
        {
            if (m_reverseQueue[i]->flags & QnAbstractMediaData::MediaFlags_ReverseReordered)
                return m_reverseQueue[i]->pkt_dts;
            else
                return AV_NOPTS_VALUE;
        }
    }
    return AV_NOPTS_VALUE;
}

void QnVideoStreamDisplay::updateRenderList()
{
    NX_MUTEX_LOCKER lock(&m_renderListMtx);
    if (m_renderListModified)
    {
        foreach (QnResourceWidgetRenderer* renderer, m_newList)
        {
            renderer->inUse();

            if (m_blockedTimeValue)
                renderer->blockTimeValue(m_channelNumber, *m_blockedTimeValue);
            else
                renderer->unblockTimeValue(m_channelNumber);
        }

        foreach (QnResourceWidgetRenderer* renderer, m_renderList)
            renderer->notInUse();

        m_renderList = m_newList;
        if (m_bufferedFrameDisplayer)
            m_bufferedFrameDisplayer->setRenderList(m_renderList);

        m_renderListModified = false;
    }
};

void QnVideoStreamDisplay::calcSampleAR(CLVideoDecoderOutputPtr outFrame, QnAbstractVideoDecoder* dec)
{
    if (!m_overridenAspectRatio.isValid())
    {
        outFrame->sample_aspect_ratio = dec->getSampleAspectRatio();
    }
    else {
        qreal realAR = outFrame->height > 0 ? (qreal) outFrame->width / (qreal) outFrame->height : 1.0;
        outFrame->sample_aspect_ratio = m_overridenAspectRatio.toFloat() / realAR;
    }
}

MultiThreadDecodePolicy QnVideoStreamDisplay::toEncoderPolicy(bool useMtDecoding) const
{
    if (!appContext()->localSettings()->allowMtDecoding())
        return MultiThreadDecodePolicy::disabled;
    if (appContext()->localSettings()->forceMtDecoding())
        return MultiThreadDecodePolicy::enabled;

    if (useMtDecoding)
        return MultiThreadDecodePolicy::enabled;
    return MultiThreadDecodePolicy::autoDetect;
}

#ifdef __QSV_SUPPORTED__

bool canAddIntel(const QnConstCompressedVideoDataPtr& data, bool reverseMode)
{
    return appContext()->localSettings()->hardwareDecodingEnabled()
        && isIntel()
        && !reverseMode
        && QuickSyncVideoDecoderOldPlayer::instanceCount()
            < appContext()->localSettings()->maxHardwareDecoders()
        && QuickSyncVideoDecoderOldPlayer::isSupported(data);
}

bool canAddNvidia(const QnConstCompressedVideoDataPtr& data, bool reverseMode)
{
    return appContext()->localSettings()->hardwareDecodingEnabled()
        && isNvidia()
        && !reverseMode
        && NvidiaVideoDecoderOldPlayer::instanceCount()
            < appContext()->localSettings()->maxHardwareDecoders()
        && NvidiaVideoDecoderOldPlayer::isSupported(data);
    return false;
}

bool QnVideoStreamDisplay::shouldUpdateDecoder(
    const QnConstCompressedVideoDataPtr& data, bool reverseMode)
{
    if (!m_decoderData.decoder)
        return false;

    if (m_decoderData.decoder->hardwareDecoder())
    {
        if (!appContext()->localSettings()->hardwareDecodingEnabled())
            return true; // Decoder should be changed to software
    }
    else
    {
        if (canAddIntel(data, reverseMode) || canAddNvidia(data, reverseMode))
            return true; // Decoder should be changed to hardware
    }

    return false;
}

#else // __QSV_SUPPORTED__

bool canAddIntel(QnConstCompressedVideoDataPtr&, bool) { return false; }
bool canAddNvidia(QnConstCompressedVideoDataPtr&, bool) { return false; }
bool QnVideoStreamDisplay::shouldUpdateDecoder(const QnConstCompressedVideoDataPtr&, bool) { return false; }

#endif // __QSV_SUPPORTED__

QnAbstractVideoDecoder* QnVideoStreamDisplay::createVideoDecoder(
    const QnConstCompressedVideoDataPtr& data, bool mtDecoding) const
{
    QnAbstractVideoDecoder* decoder = nullptr;

#ifdef __QSV_SUPPORTED__
    if (canAddIntel(data, m_reverseMode))
    {
        decoder = new QuickSyncVideoDecoderOldPlayer();
    }
    else if (canAddNvidia(data, m_reverseMode))
    {
        decoder = new NvidiaVideoDecoderOldPlayer();
    }
#endif // __QSV_SUPPORTED__
    if (!decoder)
    {
        DecoderConfig config;
        config.mtDecodePolicy = toEncoderPolicy(mtDecoding);
        config.forceGrayscaleDecoding = nx::vms::client::desktop::ini().grayscaleDecoding;
        decoder = new QnFfmpegVideoDecoder(config, /*metrics*/ nullptr, data);
    }
    decoder->setLightCpuMode(m_decodeMode);
    return decoder;
}

void QnVideoStreamDisplay::flushReverseBlock(
    QnAbstractVideoDecoder* dec,
    QnCompressedVideoDataPtr data,
    QnFrameScaler::DownscaleFactor forceScaleFactor)
{
    while (true)
    {
        CLVideoDecoderOutputPtr decodedFrame(new CLVideoDecoderOutput());
        if (!dec->decode(nullptr, &decodedFrame))
            break;

        CLVideoDecoderOutputPtr tmpOutFrame(new CLVideoDecoderOutput());
        if (!downscaleOrForward(decodedFrame, tmpOutFrame, forceScaleFactor))
            continue;

        tmpOutFrame->channel = data->channelNumber;
        tmpOutFrame->flags |= QnAbstractMediaData::MediaFlags_Reverse;
        if (data->flags & QnAbstractMediaData::MediaFlags_LowQuality)
            tmpOutFrame->flags |= QnAbstractMediaData::MediaFlags_LowQuality;

        m_reverseQueue.enqueue(tmpOutFrame);
        if (tmpOutFrame->data[0])
            m_reverseSizeInBytes += tmpOutFrame->sizeBytes();
        checkQueueOverflow();
    }
    m_flushedBeforeReverseStart = true;
    reorderPrevFrames();
    dec->resetDecoder(data);
}

QnVideoStreamDisplay::FrameDisplayStatus QnVideoStreamDisplay::display(
    QnCompressedVideoDataPtr data, bool draw, QnFrameScaler::DownscaleFactor forceScaleFactor)
{
    if (ini().disableVideoRendering)
        return Status_Displayed;

    {
        //  Clear previos frame, since decoder clear it on decode call
        NX_MUTEX_LOCKER lock(&m_lastDisplayedFrameMutex);
        if (m_lastDisplayedFrame && m_lastDisplayedFrame->isExternalData())
            m_lastDisplayedFrame.reset();
    }
    updateRenderList();

    // use only 1 frame for non selected video
    const bool reverseMode = m_reverseMode;

    if (reverseMode)
        m_lastIgnoreTime = AV_NOPTS_VALUE;
    else if (!draw)
        m_lastIgnoreTime = data->timestamp;


    m_isLive = data->flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE);
    const bool needReinitDecoders = m_needReinitDecoders.exchange(false);

    if (qAbs(m_speed - 1.0) < FPS_EPS && m_canUseBufferedFrameDisplayer)
    {
        if (!m_bufferedFrameDisplayer)
        {
            //NX_MUTEX_LOCKER lock( &m_timeMutex );
            m_bufferedFrameDisplayer = std::make_unique<QnBufferedFrameDisplayer>();
            m_bufferedFrameDisplayer->setRenderList(m_renderList);
            m_queueWasFilled = false;
        }
    }
    else
    {
        if (m_bufferedFrameDisplayer)
        {
            m_bufferedFrameDisplayer->waitForFramesDisplayed();
            //overrideTimestampOfNextFrameToRender(m_bufferedFrameDisplayer->getTimestampOfNextFrameToRender());
            //NX_MUTEX_LOCKER lock( &m_timeMutex );
            m_bufferedFrameDisplayer.reset();
        }
    }

    if (needReinitDecoders)
    {
        NX_MUTEX_LOCKER lock(&m_mtx);
        if (m_decoderData.decoder)
            m_decoderData.decoder->setMultiThreadDecodePolicy(toEncoderPolicy(m_mtDecoding));
    }

    if (data->compressionType == AV_CODEC_ID_NONE)
    {
        NX_ERROR(this, "display: unknown codec type...");
        return Status_Displayed; // true to prevent 100% cpu usage on unknown codec
    }

    if (reverseMode != m_prevReverseMode || m_needResetDecoder)
    {
        clearReverseQueue();
        NX_MUTEX_LOCKER lock( &m_mtx );
        if (reverseMode != m_prevReverseMode)
            m_decoderData.decoder.reset();

        m_prevReverseMode = reverseMode;
    }

    QnAbstractVideoDecoder* dec = nullptr;
    {
        NX_MUTEX_LOCKER lock(&m_mtx);

        // Check can we use one more HW decoder or HW accelersation is disabled.
        if (data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        {
            if (shouldUpdateDecoder(data, m_reverseMode))
                m_decoderData.decoder.reset();
        }

        dec = m_decoderData.decoder.get();
        auto frameResolution = data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey)
            ? nx::media::getFrameSize(data.get())
            : QSize();

        auto decoderResolution = dec ? QSize(dec->getWidth(), dec->getHeight()) : QSize();
        bool resolutionChanged = !frameResolution.isEmpty() && decoderResolution != frameResolution;

        if (!dec || resolutionChanged || m_decoderData.compressionType != data->compressionType)
        {
            if (!data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
                return Status_Skipped;
            NX_DEBUG(this, "Reset video decoder, resolution: %1(previous: %2), codec: %3, exist: %4",
                frameResolution, decoderResolution, data->compressionType, dec != nullptr);
            m_decoderData.decoder.reset();
            dec = createVideoDecoder(data, m_mtDecoding);
            m_decoderData.decoder.reset(dec);
            m_decoderData.compressionType = data->compressionType;
            m_needResetDecoder = false;
        }
    }

    if (m_needResetDecoder)
    {
        NX_MUTEX_LOCKER lock(&m_mtx);
        m_decoderData.decoder->resetDecoder(data);
        m_needResetDecoder = false;
    }

    m_mtx.lock();

    if (data->flags.testFlag(QnAbstractMediaData::MediaFlags_AfterEOF))
    {
        for (const auto& render: m_renderList)
            render->waitForFrameDisplayed(0);
        dec->resetDecoder(data);
    }

    if (data->flags.testFlag(QnAbstractMediaData::MediaFlags_ReverseBlockStart)
        && m_decodeMode != QnAbstractVideoDecoder::DecodeMode_Fastest)
    {
        flushReverseBlock(dec, data, forceScaleFactor);
    }

    CLVideoDecoderOutputPtr decodedFrame(new CLVideoDecoderOutput());
    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    decodedFrame->channel = data->channelNumber;
    decodedFrame->flags = {};
    if (!dec->decode(data, &decodedFrame))
    {
        m_mtx.unlock();
        if (m_decodeMode == QnAbstractVideoDecoder::DecodeMode_Fastest)
            return Status_Skipped;
        if (!m_reverseQueue.isEmpty()
            && m_reverseQueue.front()->flags.testFlag(QnAbstractMediaData::MediaFlags_ReverseReordered))
        {
            outFrame = m_reverseQueue.dequeue();
            if (outFrame->data[0])
                m_reverseSizeInBytes -= outFrame->sizeBytes();

            calcSampleAR(outFrame, dec);

            if (processDecodedFrame(dec, outFrame))
                return Status_Displayed;
            else
                return Status_Buffered;
        }
        if (m_bufferedFrameDisplayer)
            dec->setLightCpuMode(QnAbstractVideoDecoder::DecodeMode_Full);

        if (dec->getLastDecodeResult() < 0)
            return Status_Skipped;
        else
            return Status_Buffered;
    }
    {
        NX_MUTEX_LOCKER lock(&m_lastDisplayedFrameMutex);
        m_lastDisplayedFrame = decodedFrame;
    }
    m_mtx.unlock();
    m_rawDataSize = QSize(decodedFrame->width,decodedFrame->height);
    if (decodedFrame->width)
    {
        if (!m_overridenAspectRatio.isValid())
        {
            QSize imageSize(decodedFrame->width * dec->getSampleAspectRatio(), decodedFrame->height);
            NX_MUTEX_LOCKER lock( &m_imageSizeMtx );
            m_imageSize = imageSize;
        }
        else
        {
            QSize imageSize(decodedFrame->height*m_overridenAspectRatio.toFloat(),
                decodedFrame->height);
            NX_MUTEX_LOCKER lock( &m_imageSizeMtx );
            m_imageSize = imageSize;
        }
    }

    if (m_flushedBeforeReverseStart)
    {
        data->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
        m_flushedBeforeReverseStart = false;
    }

    if (!draw || m_renderList.empty())
        return Status_Skipped;
    else if (m_lastIgnoreTime != (qint64)AV_NOPTS_VALUE && decodedFrame->pkt_dts <= m_lastIgnoreTime)
        return Status_Skipped;

    if (!downscaleOrForward(decodedFrame, outFrame, forceScaleFactor))
        return Status_Displayed;

    if (reverseMode)
    {
        if (outFrame->flags.testFlag(QnAbstractMediaData::MediaFlags_ReverseBlockStart))
            reorderPrevFrames();
        m_reverseQueue.enqueue(outFrame);
        if (outFrame->data[0])
            m_reverseSizeInBytes += outFrame->sizeBytes();
        checkQueueOverflow();

        while(!m_reverseQueue.isEmpty() && !m_reverseQueue.front()->data[0])
            outFrame = m_reverseQueue.dequeue();

        if (m_reverseQueue.isEmpty())
            return Status_Skipped;

        if (!m_reverseQueue.front()->flags.testFlag(QnAbstractMediaData::MediaFlags_ReverseReordered))
            return Status_Buffered; // frame does not ready. need more frames. does not perform wait
        outFrame = m_reverseQueue.dequeue();
        if (outFrame->data[0])
            m_reverseSizeInBytes -= outFrame->sizeBytes();

        // TODO In reverse mode we use downscaled frames to get frame that currently rendered?
        // Or we need to keep original frames in queue.
        NX_MUTEX_LOCKER lock(&m_lastDisplayedFrameMutex);
        m_lastDisplayedFrame = outFrame;
    }

    calcSampleAR(outFrame, dec);
    if (processDecodedFrame(dec, outFrame))
        return Status_Displayed;
    else
        return Status_Buffered;
}

bool QnVideoStreamDisplay::downscaleOrForward(
    const CLConstVideoDecoderOutputPtr& src,
    CLVideoDecoderOutputPtr& dst,
    QnFrameScaler::DownscaleFactor forceScaleFactor)
{
    auto scaleFactor = determineScaleFactor(
        m_renderList, src->channel, src->width, src->height, forceScaleFactor);

    AVPixelFormat pixFmt = (AVPixelFormat)src->format;
    const bool needScale =
        (src->memoryType() == MemoryType::SystemMemory) &&
        (!QnGLRenderer::isPixelFormatSupported(pixFmt) ||
            !CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) ||
            scaleFactor != QnFrameScaler::factor_1);

    if (!needScale)
    {
        //dst = src;
        dst = src.constCast<CLVideoDecoderOutput>();
        dst->scaleFactor = scaleFactor;
        return true;
    }

    if (QnGLRenderer::isPixelFormatSupported(pixFmt)
        && CLVideoDecoderOutput::isPixelFormatSupported(pixFmt)
        && scaleFactor <= QnFrameScaler::factor_8)
    {
        QnFrameScaler::downscale(src.data(), dst.data(), scaleFactor); // fast scaler
    }
    else
    {
        // Universal scaler.
        if (!rescaleFrame(*(src.data()), *dst, src->width / scaleFactor, src->height / scaleFactor))
            return false;
    }
    dst->pkt_dts = src->pkt_dts;
    dst->pkt_pts = src->pkt_pts;
    dst->flags = src->flags;
    dst->channel = src->channel;
    dst->scaleFactor = scaleFactor;
    return true;
}

QnVideoStreamDisplay::FrameDisplayStatus QnVideoStreamDisplay::flushFrame(
    int channel,
    QnFrameScaler::DownscaleFactor forceScaleFactor)
{
    {
        //  Clear previos frame, since decoder clear it on decode call
        NX_MUTEX_LOCKER lock(&m_lastDisplayedFrameMutex);
        if (m_lastDisplayedFrame && m_lastDisplayedFrame->isExternalData())
            m_lastDisplayedFrame.reset();
    }

    // use only 1 frame for non selected video
    if (m_reverseMode || !m_decoderData.decoder || m_needResetDecoder)
        return Status_Skipped;

    CLVideoDecoderOutputPtr decodedFrame(new CLVideoDecoderOutput());
    QnAbstractVideoDecoder* dec = m_decoderData.decoder.get();

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    for (auto& render: m_renderList)
        render->finishPostedFramesRender(channel);
    outFrame->channel = channel;
    {
        NX_MUTEX_LOCKER lock(&m_mtx);
        if (!dec->decode(QnCompressedVideoDataPtr(), &decodedFrame))
            return Status_Skipped;
    }

    if (!downscaleOrForward(decodedFrame, outFrame, forceScaleFactor))
        return Status_Displayed;

    calcSampleAR(outFrame, dec);
    if (processDecodedFrame(dec, outFrame))
        return Status_Displayed;
    else
        return Status_Buffered;
}

bool QnVideoStreamDisplay::processDecodedFrame(
    QnAbstractVideoDecoder* decoder,
    const CLConstVideoDecoderOutputPtr& outFrame)
{
    if (outFrame->isEmpty())
        return false;

    if (m_isLive && outFrame->memoryType() != MemoryType::VideoMemory)
    {
        auto camera = m_resource->toResourcePtr();
        auto systemContext = SystemContext::fromResource(camera);
        systemContext->videoCache()->add(camera->getId(), outFrame);
    }

    NX_ASSERT(!outFrame->isExternalData());

    if (m_bufferedFrameDisplayer)
    {
        const bool wasWaiting = m_bufferedFrameDisplayer->addFrame(outFrame);
        const qint64 bufferedDuration = m_bufferedFrameDisplayer->bufferedDuration();
        if (wasWaiting)
        {
            decoder->setLightCpuMode(QnAbstractVideoDecoder::DecodeMode_Full);
            m_queueWasFilled = true;
        }
        else
        {
            if (m_queueWasFilled && bufferedDuration <= kMaxQueueTime / 4)
                decoder->setLightCpuMode(QnAbstractVideoDecoder::DecodeMode_Fast);
        }
    }
    else
    {
        if (std::abs(static_cast<double>(m_speed)) < 1.0 + FPS_EPS)
        {
            for (const auto render: m_renderList)
            {
                // Wait for old frame.
                render->waitForQueueLessThan(outFrame->channel, kMaxFrameQueueSize);
            }
        }
        for (const auto render: m_renderList)
            render->draw(outFrame, getMaxScreenSizeUnsafe()); //< Send the new one.
    }
    return true;
}

bool QnVideoStreamDisplay::selfSyncUsed() const
{
    return m_bufferedFrameDisplayer != nullptr;
}

QnAspectRatio QnVideoStreamDisplay::overridenAspectRatio() const
{
    return m_overridenAspectRatio;
}

void QnVideoStreamDisplay::setOverridenAspectRatio(QnAspectRatio aspectRatio)
{
    m_overridenAspectRatio = aspectRatio;
}

void QnVideoStreamDisplay::flushFramesToRenderer()
{
    for (const auto& render: m_renderList)
        render->finishPostedFramesRender( m_channelNumber );
}

bool QnVideoStreamDisplay::rescaleFrame(
    const CLVideoDecoderOutput& srcFrame,
    CLVideoDecoderOutput& outFrame,
    int newWidth,
    int newHeight)
{
    static const int ROUND_FACTOR = 16;
    // due to openGL requirements chroma MUST be divided by 4, luma MUST be divided by 8
    // due to MMX scaled functions requirements chroma MUST be divided by 8, so luma MUST be divided by 16
    newWidth = qPower2Ceil((unsigned)newWidth, ROUND_FACTOR);

    if (m_scaleContext != 0 && (m_outputWidth != newWidth || m_outputHeight != newHeight))
    {
        freeScaleContext();
        if (!allocScaleContext(srcFrame, newWidth, newHeight))
            return false;
    }
    else if (m_scaleContext == 0)
    {
        if (!allocScaleContext(srcFrame, newWidth, newHeight))
            return false;
    }

    if (outFrame.isExternalData() || outFrame.width != newWidth || outFrame.height != newHeight || outFrame.format != AV_PIX_FMT_RGBA)
        outFrame.reallocate(newWidth, newHeight, AV_PIX_FMT_RGBA);

    sws_scale(m_scaleContext,srcFrame.data, srcFrame.linesize, 0,
        srcFrame.height, outFrame.data, outFrame.linesize);
    return true;
}

void QnVideoStreamDisplay::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    m_decodeMode = val;
    NX_MUTEX_LOCKER mutex( &m_mtx );

    if (m_decoderData.decoder)
        m_decoderData.decoder->setLightCpuMode(val);
}

void QnVideoStreamDisplay::setMTDecoding(bool value)
{
    m_mtDecoding = value;
    m_needReinitDecoders = true;
}

void QnVideoStreamDisplay::setSpeed(float value)
{
    m_speed = value;
    m_reverseMode = value < 0;
}

void QnVideoStreamDisplay::overrideTimestampOfNextFrameToRender(qint64 value)
{
    for (const auto& render: m_renderList)
    {
        render->blockTimeValue(m_channelNumber, microseconds(value));
        if (m_bufferedFrameDisplayer)
            m_bufferedFrameDisplayer->clear();
        render->finishPostedFramesRender(m_channelNumber);
        render->unblockTimeValue(m_channelNumber);
    }
}

qint64 QnVideoStreamDisplay::getTimestampOfNextFrameToRender() const
{
    if (m_renderList.empty())
        return AV_NOPTS_VALUE;
    for (const auto& renderer: m_renderList)
    {
        if (renderer->isEnabled(m_channelNumber))
            return renderer->getTimestampOfNextFrameToRender(m_channelNumber).count();
    }

    QnResourceWidgetRenderer* renderer = *m_renderList.begin();
    return renderer->getTimestampOfNextFrameToRender(m_channelNumber).count();
}

void QnVideoStreamDisplay::blockTimeValue(qint64 time)
{
    m_blockedTimeValue = microseconds(time);

    foreach (QnResourceWidgetRenderer* renderer, m_renderList)
        renderer->blockTimeValue(m_channelNumber, *m_blockedTimeValue);
}

void QnVideoStreamDisplay::setPausedSafe(bool value)
{
    NX_MUTEX_LOCKER lock( &m_renderListMtx );
    foreach(QnResourceWidgetRenderer* render, m_renderList)
        render->setPaused(value);
    for (const auto& render: m_newList)
        render->setPaused(value);
    m_isPaused = value;
}

void QnVideoStreamDisplay::blockTimeValueSafe(qint64 time)
{
    NX_MUTEX_LOCKER lock(&m_renderListMtx);
    blockTimeValue(time);
}

bool QnVideoStreamDisplay::isTimeBlocked() const
{
    NX_MUTEX_LOCKER lock(&m_renderListMtx);
    return (bool) m_blockedTimeValue;
}

void QnVideoStreamDisplay::unblockTimeValue()
{
    m_blockedTimeValue = {};

    for (const auto& render: m_renderList)
        render->unblockTimeValue(m_channelNumber);
}

void QnVideoStreamDisplay::afterJump()
{
    clearReverseQueue();
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->clear();
    for (const auto& render: m_renderList)
        render->finishPostedFramesRender(m_channelNumber);
    m_needResetDecoder = true;
    m_queueWasFilled = false;
    m_lastIgnoreTime = AV_NOPTS_VALUE;
}

void QnVideoStreamDisplay::clearReverseQueue()
{
    for (const auto& render: m_renderList)
        render->finishPostedFramesRender(0);

    NX_MUTEX_LOCKER lock( &m_mtx );
    m_reverseQueue.clear();
    m_reverseSizeInBytes = 0;
}

QImage QnVideoStreamDisplay::getGrayscaleScreenshot()
{
    NX_MUTEX_LOCKER mutex( &m_lastDisplayedFrameMutex );

    const AVFrame* frame = m_lastDisplayedFrame.data();
    if (!frame || !frame->width || !frame->data[0])
        return QImage();

    QImage tmp(frame->data[0],
        frame->width,
        frame->height,
        frame->linesize[0],
        QImage::Format_Indexed8);
    QImage rez(frame->width, frame->height, QImage::Format_Indexed8);
    rez = tmp.copy(0,0, frame->width, frame->height);
    return rez;
}

CLVideoDecoderOutputPtr QnVideoStreamDisplay::getScreenshot(bool anyQuality)
{
    NX_MUTEX_LOCKER mutex(&m_lastDisplayedFrameMutex);
    if (!m_lastDisplayedFrame || m_lastDisplayedFrame->isEmpty())
        return CLVideoDecoderOutputPtr();

    // feature #2563
    if (!anyQuality && m_lastDisplayedFrame->flags.testFlag(QnAbstractMediaData::MediaFlags_LowQuality))
        return CLVideoDecoderOutputPtr(); //screenshot will be received from the server

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    outFrame->copyFrom(m_lastDisplayedFrame.data());
    NX_DEBUG(this, "Got screenshot with resolution: %1", outFrame->size());
    return outFrame;
}

void QnVideoStreamDisplay::setCurrentTime(qint64 time)
{
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->setCurrentTime(time);
}

void QnVideoStreamDisplay::canUseBufferedFrameDisplayer(bool value)
{
    m_canUseBufferedFrameDisplayer = value;
}

QSize QnVideoStreamDisplay::getImageSize() const
{
    NX_MUTEX_LOCKER lock( &m_imageSizeMtx );
    return m_imageSize;
}

QSize QnVideoStreamDisplay::getMaxScreenSizeUnsafe() const
{
    int maxW = 0, maxH = 0;
    for (const auto& render: m_renderList)
    {
        QSize sz = render->sizeOnScreen(0);
        maxW = qMax(sz.width(), maxW);
        maxH = qMax(sz.height(), maxH);
    }
    return QSize(maxW, maxH);
}

QSize QnVideoStreamDisplay::getMaxScreenSize() const
{
    NX_MUTEX_LOCKER lock(&m_renderListMtx);
    return getMaxScreenSizeUnsafe();
}
