////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifdef Q_OS_LINUX

#ifndef XVBADECODERPLUGIN_H
#define XVBADECODERPLUGIN_H

#include <plugins/videodecoders/abstractvideodecoderusagecalculator.h>
#include <plugins/videodecoders/pluginusagewatcher.h>
#include <decoders/abstractvideodecoderplugin.h>


//!Provides video decoder accelerated with AMD XVBA API
class QnXVBADecoderPlugin
:
    public QObject,
    public QnAbstractVideoDecoderPlugin
{
    Q_OBJECT
    Q_INTERFACES(QnAbstractVideoDecoderPlugin)

public:
    //!Implementation of QnAbstractClientPlugin::minSupportedVersion
    virtual quint32 minSupportedVersion() const;
    //!Implementation of QnAbstractDecoderPlugin::supportedCodecTypes
    virtual QList<CodecID> supportedCodecTypes() const;
    //!Implementation of QnAbstractDecoderPlugin::isHardwareAccelerated
    virtual bool isHardwareAccelerated() const;
    //!Implementation of QnAbstractDecoderPlugin::create
    virtual QnAbstractVideoDecoder* create(
            CodecID codecID,
            const QnCompressedVideoDataPtr& data,
            const QGLContext* const glContext ) const;
};

#endif  //XVBADECODERPLUGIN_H

#endif
