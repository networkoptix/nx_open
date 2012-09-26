////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTVIDEODECODERPLUGIN_H
#define ABSTRACTVIDEODECODERPLUGIN_H

#include <QGLContext>
#include <QList>

#include <libavcodec/avcodec.h>

#include "abstractclientplugin.h"
#include "../core/datapacket/media_data_packet.h"


class QnAbstractVideoDecoder;

//!Base class for video decoder plugins. Such plugin can be system-dependent and can provide hardware-acceleration of decoding
class QnAbstractVideoDecoderPlugin
:
    public QnAbstractClientPlugin
{
public:
    //!Returns list of MIME types of supported formats
    virtual QList<CodecID> supportedCodecTypes() const = 0;
    //!Returns true, if decoder CAN offer hardware acceleration
    /*!
        \note Actually, ability of hardware acceleration depends on stream parameters: resolution, bitrate, profile/level etc.
            And can only be found out by creation of decoder and parsing media stream header
    */
    virtual bool isHardwareAccelerated() const = 0;
    //!Creates decoder object with operator \a new
    /*!
        \param codecID Codec type
        \param data Access unit of media stream. MUST contain sequence header (sps & pps in case of h.264)
        \note QnAbstractVideoDecoder::decode method of created object MUST be called from same thread that called this method
    */
    virtual QnAbstractVideoDecoder* create(
            CodecID codecID,
            const QnCompressedVideoDataPtr& data,
            const QGLContext* const glContext ) const = 0;
};

Q_DECLARE_INTERFACE( QnAbstractVideoDecoderPlugin, "com.networkoptix.Plugin.XVBADecoder/0.1" );

#endif //ABSTRACTVIDEODECODERPLUGIN_H
