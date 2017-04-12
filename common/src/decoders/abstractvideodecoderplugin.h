////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTVIDEODECODERPLUGIN_H
#define ABSTRACTVIDEODECODERPLUGIN_H

#ifdef ENABLE_DATA_PROVIDERS

//#include <QtOpenGL/QGLContext>
#include <QtCore/QList>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "abstractclientplugin.h"
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/stree/resourcecontainer.h>


class QGLContext;
class QnAbstractVideoDecoder;

//!Base class for video decoder plugins. Such plugin can be system-dependent and can provide hardware-acceleration of decoding
class QnAbstractVideoDecoderPlugin
:
    public QnAbstractClientPlugin
{
public:
    virtual ~QnAbstractVideoDecoderPlugin() {}

    //!Returns list of MIME types of supported formats
    virtual QList<AVCodecID> supportedCodecTypes() const = 0;
    //!Returns true, if decoder CAN offer hardware acceleration. It does not mean it can decode every stream with hw acceleration
    /*!
        \note Actually, ability of hardware acceleration depends on stream parameters: resolution, bitrate, profile/level etc.
            And can only be found out by instantiation of decoder and parsing media stream header
    */
    virtual bool isHardwareAccelerated() const = 0;
    //!Creates decoder object with operator \a new
    /*!
        \param codecID Codec type
        \param data Access unit of media stream. MUST contain sequence header (sps & pps in case of h.264)
        \param glContext Parent OpenGL context to use. In case decoder returns decoded pictures as gl textures, these textures MUST belong to gl context shared with \a glContext
        \param currentSWDecoderCount
        \note QnAbstractVideoDecoder::decode method of created object MUST be called from same thread that called this method
    */
    virtual QnAbstractVideoDecoder* create(
            AVCodecID codecID,
            const QnCompressedVideoDataPtr& data,
            const QGLContext* const glContext,
            int currentSWDecoderCount ) const = 0;
    virtual bool isStreamSupported( const nx::utils::stree::AbstractResourceReader& newStreamParams ) const = 0;
};

Q_DECLARE_INTERFACE( QnAbstractVideoDecoderPlugin, "com.networkoptix.plugin.videodecoder/0.1" );

#endif // ENABLE_DATA_PROVIDERS

#endif //ABSTRACTVIDEODECODERPLUGIN_H
