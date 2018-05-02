#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <decoders/video/abstract_video_decoder.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>

class PluginManager;

class QnVideoDecoderFactory
{
public:
    // TODO: #Elric #enum
    enum CLCodecManufacture
    {
        FFMPEG,
        INTELIPP,
        //!select most apropriate decoder (gives preference to hardware one, if available)
        AUTO
    };

    /*!
    \param data Packet of source data. MUST contain media stream sequence header
    \param mtDecoding This hint tells that decoder is allowed (not have to) to perform multi-threaded decoding
    \param glContext OpenGL context used to draw to screen. Decoder, that renders pictures directly to opengl texture,
    MUST be aware of application gl context to create textures shared with this context
    \param allowHardwareDecoding If true, method will try to find loaded hardware decoder plugin with support of requested stream type.
    Otherwise, it will ignore any loaded decoder plugin
    */
    static QnAbstractVideoDecoder* createDecoder(
        const QnCompressedVideoDataPtr& data,
        bool mtDecoding,
        const QGLContext* glContext = NULL,
        bool allowHardwareDecoding = false );

    static void setCodecManufacture( CLCodecManufacture codecman )
    {
        m_codecManufacture = codecman;
    }

    static void setPluginManager(PluginManager* pluginManager)
    {
        m_pluginManager = pluginManager;
    }

private:
    static CLCodecManufacture m_codecManufacture;
    static PluginManager* m_pluginManager;
};

#endif // ENABLE_DATA_PROVIDERS
