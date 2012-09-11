////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include "xvbadecoderplugin.h"

#include "xvbadecoder.h"


quint32 QnXVBADecoderPlugin::minSupportedVersion() const
{
    //TODO/IMPL
    return 1<<24;   //1.0.0
}

QList<CodecID> QnXVBADecoderPlugin::supportedCodecTypes() const
{
    QList<CodecID> codecList;
    codecList.push_back( CODEC_ID_H264 );
    return codecList;
}

bool QnXVBADecoderPlugin::isHardwareAccelerated() const
{
    return true;
}

QnAbstractVideoDecoder* QnXVBADecoderPlugin::create(
        CodecID codecID,
        const QnCompressedVideoDataPtr& data,
        const QGLContext* const glContext ) const
{
    return new QnXVBADecoder( glContext, data );
}

Q_EXPORT_PLUGIN2( xvbaplugin, QnXVBADecoderPlugin );
