////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef XVBADECODERPLUGIN_H
#define XVBADECODERPLUGIN_H

#include <memory>

#include <QObject>

#include <plugins/videodecoders/abstractvideodecoderusagecalculator.h>
#include <plugins/videodecoders/pluginusagewatcher.h>
#include <plugins/videodecoders/stree/resourcenameset.h>
#include <decoders/abstractvideodecoderplugin.h>

#include "linuxgraphicsadapterdescription.h"


//!Provides video decoder accelerated with AMD XVBA API
class QnXVBADecoderPlugin
:
    public QObject,
    public QnAbstractVideoDecoderPlugin
{
    Q_OBJECT
    Q_INTERFACES(QnAbstractVideoDecoderPlugin)

public:
    QnXVBADecoderPlugin();

    //!Implementation of QnAbstractClientPlugin::minSupportedVersion
    virtual quint32 minSupportedVersion() const;
    //!Implementation of QnAbstractClientPlugin::initializeLog
    virtual void initializeLog( QnLog* );

    //!Implementation of QnAbstractDecoderPlugin::supportedCodecTypes
    virtual QList<AVCodecID> supportedCodecTypes() const;
    //!Implementation of QnAbstractDecoderPlugin::isHardwareAccelerated
    virtual bool isHardwareAccelerated() const;
    //!Implementation of QnAbstractDecoderPlugin::create
    virtual QnAbstractVideoDecoder* create(
            AVCodecID codecID,
            const QnCompressedVideoDataPtr& data,
            const QGLContext* const glContext ) const;

private:
    DecoderResourcesNameset m_resNameset;
    mutable std::unique_ptr<PluginUsageWatcher> m_usageWatcher;
    mutable std::unique_ptr<AbstractVideoDecoderUsageCalculator> m_usageCalculator;
    mutable std::unique_ptr<LinuxGraphicsAdapterDescription> m_graphicsDesc;
    QString m_sdkVersionStr;
    bool m_hardwareAccelerationEnabled;
};

#endif  //XVBADECODERPLUGIN_H
