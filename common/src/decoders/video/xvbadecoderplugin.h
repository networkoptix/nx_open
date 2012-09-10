////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef XVBADECODERPLUGIN_H
#define XVBADECODERPLUGIN_H

#include "../abstractvideodecoderplugin.h"


//!Provides video decoder accelerated with AMD XVBA API
class QnXVBADecoderPlugin
:
    public QnAbstractVideoDecoderPlugin
{
    Q_OBJECT

public:
    //!Implementation of QnAbstractDecoderPlugin::supportedCodecTypes
    virtual std::list<CodecID> supportedCodecTypes() const;
    //!Implementation of QnAbstractDecoderPlugin::isHardwareAccelerated
    virtual bool isHardwareAccelerated() const;
    //!Implementation of QnAbstractDecoderPlugin::create
    virtual QnAbstractVideoDecoder* create( CodecID codecID, const QnCompressedVideoDataPtr& data ) const;
};

#endif  //XVBADECODERPLUGIN_H
