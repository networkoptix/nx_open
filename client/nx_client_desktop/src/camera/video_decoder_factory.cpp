#if defined(ENABLE_DATA_PROVIDERS)

#include "video_decoder_factory.h"

#include <iostream>
#include <memory>

#include <decoders/abstractvideodecoderplugin.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <decoders/video/ipp_h264_decoder.h>

#include <nx/utils/log/log.h>
#include <plugins/plugin_manager.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}


QnVideoDecoderFactory::CLCodecManufacture QnVideoDecoderFactory::m_codecManufacture = AUTO;
PluginManager* QnVideoDecoderFactory::m_pluginManager = nullptr;

QnAbstractVideoDecoder* QnVideoDecoderFactory::createDecoder(
    const QnCompressedVideoDataPtr& data,
    bool mtDecoding,
    const QGLContext* glContext,
    bool allowHardwareDecoding)
{
    //TODO/IMPL this is a quick solution. Need something more beautiful (static counter is not beautiful at all...)
    static QAtomicInt swDecoderCount = 0;

    // h264 
    switch (m_codecManufacture)
    {
#ifdef Q_OS_WIN
        case INTELIPP:
            return new IPPH264Decoder();
#endif

        case AUTO:
        {
            if (allowHardwareDecoding)
            {
                //searching for a video decoder with hardware acceleration, supporting codec type data->compressionType
                QnAbstractVideoDecoderPlugin* videoDecoderPlugin = NULL;
                if (!m_pluginManager)
                    return nullptr;

                const auto& plugins = m_pluginManager->findPlugins<QnAbstractVideoDecoderPlugin>();
                for (QnAbstractVideoDecoderPlugin* plugin : plugins)
                {
                    if (!plugin->isHardwareAccelerated() || plugin->supportedCodecTypes().indexOf(data->compressionType) == -1)
                        continue;
                    videoDecoderPlugin = plugin;
                    break;
                }
                if (videoDecoderPlugin)
                {
                    std::unique_ptr<QnAbstractVideoDecoder> decoder(
                        videoDecoderPlugin->create(
                            data->compressionType, data, glContext, swDecoderCount.load()));

                    if (decoder.get() && decoder->isHardwareAccelerationEnabled())
                        return decoder.release();
                }
                NX_LOG(lit("Hardware acceleration is not supported. Switching to software decoding..."), cl_logWARNING);
            }
        }

        case FFMPEG:
        default:
        {
            return new QnFfmpegVideoDecoder(data->compressionType, data, mtDecoding, &swDecoderCount);
        }
    }

    return nullptr;
}

#endif // ENABLE_DATA_PROVIDERS
