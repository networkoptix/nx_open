#include "videostreamdisplay.h"

#include "utils/ffmpeg/ffmpeg_global.h"
#include "utils/common/log.h"
#include "decoders/abstractvideodecoder.h"
#include "resource/media_resource.h"
#include "abstractrenderer.h"
#include "gl_renderer.h"

extern "C" {
#include <libswscale/swscale.h>
}

CLVideoStreamDisplay::CLVideoStreamDisplay(CLAbstractRenderer *renderer, bool canDownscale) :
    m_renderer(renderer),
    m_canDownscale(canDownscale),
    m_prevFactor(CLVideoDecoderOutput::factor_1),
    m_scaleFactor(CLVideoDecoderOutput::factor_1),
    m_previousOnScreenSize(0,0),
    m_frameYUV(0),
    m_buffer(0),
    m_scaleContext(0),
    m_outputWidth(0),
    m_outputHeight(0)
{
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
    QMutexLocker mutex(&m_mtx);

    qDeleteAll(m_decoder);

    freeScaleContext();
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::getCurrentDownscaleFactor() const
{
    return m_scaleFactor;
}

bool CLVideoStreamDisplay::allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    m_outputWidth = newWidth;
    m_outputHeight = newHeight;

    m_scaleContext = sws_getContext(outFrame.width, outFrame.height, outFrame.out_type,
                                    m_outputWidth, m_outputHeight, PIX_FMT_RGBA,
                                    SWS_POINT, NULL, NULL, NULL);

    if (m_scaleContext == 0)
    {
        cl_log.log(QLatin1String("Can't get swscale context"), cl_logERROR);
        return false;
    }

    m_frameYUV = avcodec_alloc_frame();

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, m_outputWidth, m_outputHeight);
    m_outFrame.out_type = PIX_FMT_RGBA;

    m_buffer = (quint8*)av_malloc(numBytes * sizeof(quint8));

    avpicture_fill((AVPicture *)m_frameYUV, m_buffer, PIX_FMT_RGBA, m_outputWidth, m_outputHeight);

    return true;
}

void CLVideoStreamDisplay::freeScaleContext()
{
    if (m_scaleContext) {
        sws_freeContext(m_scaleContext);
        av_free(m_buffer);
        av_free(m_frameYUV);
        m_scaleContext = 0;
    }
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::determineScaleFactor(QnCompressedVideoDataPtr data, const CLVideoData& img, CLVideoDecoderOutput::downscale_factor force_factor)
{
    if (m_renderer->constantDownscaleFactor())
       force_factor = CLVideoDecoderOutput::factor_2;

    if (force_factor==CLVideoDecoderOutput::factor_any) // if nobody pushing lets peek it
    {
        QSize on_screen = m_renderer->sizeOnScreen(data->channelNumber);

        m_scaleFactor = findScaleFactor(img.outFrame.width, img.outFrame.height, on_screen.width(), on_screen.height());

        if (m_scaleFactor < m_prevFactor)
        {
            // new factor is less than prev one; about to change factor => about to increase resource usage
            if (qAbs((qreal)on_screen.width() - m_previousOnScreenSize.width())/on_screen.width() < 0.05 &&
                qAbs((qreal)on_screen.height() - m_previousOnScreenSize.height())/on_screen.height() < 0.05)
            {
                m_scaleFactor = m_prevFactor; // hold bigger factor ( smaller image )
            }

            // why?
            // we need to do so ( introduce some histerezis )coz downscaling changes resolution not proportionally some time( cut vertical size a bit )
            // so it may be a loop downscale => changed aspectratio => upscale => changed aspectratio => downscale.
        }

        if (m_scaleFactor != m_prevFactor)
        {
            m_previousOnScreenSize = on_screen;
            m_prevFactor = m_scaleFactor;
        }
    }
    else
    {
        m_scaleFactor = force_factor;
    }

    CLVideoDecoderOutput::downscale_factor rez = m_canDownscale ? qMax(m_scaleFactor, CLVideoDecoderOutput::factor_1) : CLVideoDecoderOutput::factor_1;
    // If there is no scaling needed check if size is greater than maximum allowed image size (maximum texture size for opengl).
    int newWidth = img.outFrame.width / rez;
    int newHeight = img.outFrame.height / rez;
    int maxTextureSize = CLGLRenderer::getMaxTextureSize();
    while (maxTextureSize > 0 && newWidth > maxTextureSize || newHeight > maxTextureSize)
    {
        rez = CLVideoDecoderOutput::downscale_factor ((int)rez * 2);
        newWidth /= 2;
        newHeight /= 2;
    }
    return rez;
}

void CLVideoStreamDisplay::dispay(QnCompressedVideoDataPtr data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor)
{
    if (data->compressionType == CODEC_ID_NONE)
    {
        cl_log.log(QLatin1String("CLVideoStreamDisplay::dispay: unknown codec type..."), cl_logERROR);
        return;
    }

    CLVideoData img;

    img.inBuffer = (unsigned char*)(data->data.data()); // :-)
    img.bufferLength = data->data.size();
    img.keyFrame = data->keyFrame;
    img.useTwice = data->useTwice;
    img.width = data->width;
    img.height = data->height;
    img.codec = data->compressionType;

    CLAbstractVideoDecoder *decoder;
    {
        QMutexLocker mutex(&m_mtx);

        decoder = m_decoder.value(data->compressionType);
        if (!decoder) {
            decoder = CLVideoDecoderFactory::createDecoder(data->compressionType, data->context);
            if (decoder)
                m_decoder.insert(data->compressionType, decoder);
        }
    }

    if (!decoder || !decoder->decode(img))
    {
        CL_LOG(cl_logDEBUG2) cl_log.log(QLatin1String("CLVideoStreamDisplay::dispay: decoder cannot decode image..."), cl_logDEBUG2);
        return;
    }

    if (!draw || !m_renderer)
        return;

    int maxTextureSize = CLGLRenderer::getMaxTextureSize();
    Q_UNUSED(maxTextureSize);

    CLVideoDecoderOutput::downscale_factor scaleFactor = determineScaleFactor(data, img, force_factor);

    if (!CLGLRenderer::isPixelFormatSupported(img.outFrame.out_type) ||
        scaleFactor > CLVideoDecoderOutput::factor_8 ||
        (scaleFactor > CLVideoDecoderOutput::factor_1 && !CLVideoDecoderOutput::isPixelFormatSupported(img.outFrame.out_type)))
    {
        rescaleFrame(img.outFrame, img.outFrame.width / scaleFactor, img.outFrame.height / scaleFactor);
        m_renderer->draw(m_outFrame, data->channelNumber);
    }
    else if (scaleFactor > CLVideoDecoderOutput::factor_1)
    {
        CLVideoDecoderOutput::downscale(&img.outFrame, &m_outFrame, scaleFactor); // extra cpu work but less to display( for weak video cards )
        m_renderer->draw(m_outFrame, data->channelNumber);
    }
    else
    {
        m_renderer->draw(img.outFrame, data->channelNumber);
    }
}

bool CLVideoStreamDisplay::rescaleFrame(CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    static const int ROUND_FACTOR = 16;
    // due to openGL requirements chroma MUST be devided by 4, luma MUST be devided by 8
    // due to MMX scaled functions requirements chroma MUST be devided by 8, so luma MUST be devided by 16
    newWidth = (newWidth / ROUND_FACTOR ) * ROUND_FACTOR;

    if (m_scaleContext != 0 && (m_outputWidth != newWidth || m_outputHeight != newHeight))
    {
        freeScaleContext();
        if (!allocScaleContext(outFrame, newWidth, newHeight))
            return false;
    }
    else if (m_scaleContext == 0)
    {
        if (!allocScaleContext(outFrame, newWidth, newHeight))
            return false;
    }

    const quint8* const srcSlice[] = {outFrame.C1, outFrame.C2, outFrame.C3};
    int srcStride[] = {outFrame.stride1, outFrame.stride2, outFrame.stride3};

    sws_scale(m_scaleContext,srcSlice, srcStride, 0,
        outFrame.height, m_frameYUV->data, m_frameYUV->linesize);

    m_outFrame.width = m_outputWidth;
    m_outFrame.height = m_outputHeight;

    m_outFrame.C1 = m_frameYUV->data[0];
    m_outFrame.C2 = m_frameYUV->data[1];
    m_outFrame.C3 = m_frameYUV->data[2];

    m_outFrame.stride1 = m_frameYUV->linesize[0];
    m_outFrame.stride2 = m_frameYUV->linesize[1];
    m_outFrame.stride3 = m_frameYUV->linesize[2];

    return true;
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::findScaleFactor(int width, int height, int fitWidth, int fitHeight)
{
    if (fitWidth * 8 <= width && fitHeight * 8 <= height)
        return CLVideoDecoderOutput::factor_8;

    if (fitWidth * 4 <= width && fitHeight * 4 <= height)
        return CLVideoDecoderOutput::factor_4;

    if (fitWidth * 2 <= width && fitHeight * 2 <= height)
        return CLVideoDecoderOutput::factor_2;

    return CLVideoDecoderOutput::factor_1;
}

void CLVideoStreamDisplay::setMTDecoding(bool value)
{
    foreach (CLAbstractVideoDecoder *decoder, m_decoder)
        decoder->setMTDecoding(value);
}
