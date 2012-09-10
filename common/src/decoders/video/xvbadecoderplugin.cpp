////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#include "xvbadecoderplugin.h"

#include "xvbadecoder.h"


std::list<CodecID> QnXVBADecoderPlugin::supportedCodecTypes() const
{
    std::list<CodecID> codecList;
    codecList.push_back( CODEC_ID_H264 );
    return codecList;
}

bool QnXVBADecoderPlugin::isHardwareAccelerated() const
{
    return true;
}

QnAbstractVideoDecoder* QnXVBADecoderPlugin::create( CodecID codecID, const QnCompressedVideoDataPtr& data ) const
{
    return new QnXVBADecoder( NULL, data );
}
