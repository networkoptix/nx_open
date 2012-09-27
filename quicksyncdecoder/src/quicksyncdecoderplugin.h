////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef QUICKSYNCDECODERPLUGIN_H
#define QUICKSYNCDECODERPLUGIN_H

#include <memory>

#include <plugins/videodecoders/abstractvideodecoderusagecalculator.h>
#include <plugins/videodecoders/pluginusagewatcher.h>
#include <decoders/abstractvideodecoderplugin.h>


//!Plugin of Intel Media SDK (Quicksync) based h.264 decoder
class QuicksyncDecoderPlugin
:
    public QObject,
    public QnAbstractVideoDecoderPlugin
{
    Q_OBJECT
    Q_INTERFACES(QnAbstractVideoDecoderPlugin)

public:
    QuicksyncDecoderPlugin();

    //!Implementation of QnAbstractClientPlugin::minSupportedVersion
    virtual quint32 minSupportedVersion() const;

    //!Implementation of QnAbstractVideoDecoderPlugin::supportedCodecTypes
    virtual QList<CodecID> supportedCodecTypes() const;
    //!Implementation of QnAbstractVideoDecoderPlugin::isHardwareAccelerated
    virtual bool isHardwareAccelerated() const;
    //!Implementation of QnAbstractVideoDecoderPlugin::create
    virtual QnAbstractVideoDecoder* create(
        CodecID codecID,
        const QnCompressedVideoDataPtr& data,
        const QGLContext* const glContext ) const;

private:
    std::auto_ptr<PluginUsageWatcher> m_usageWatcher;
    std::auto_ptr<AbstractVideoDecoderUsageCalculator> m_usageCalculator;
};

#endif  //QUICKSYNCDECODERPLUGIN_H
