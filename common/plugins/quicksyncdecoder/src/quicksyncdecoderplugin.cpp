////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "quicksyncdecoderplugin.h"

#include <QtPlugin>

#include <common/predefinedusagecalculator.h>
#include <common/videodecoderswitcher.h>

#include "quicksyncvideodecoder.h"


static const char* QUICKSYNC_PLUGIN_ID = "45D92FCC-2B59-431e-BFF9-E11F2D6213DA";
//{ 0x45d92fcc, 0x2b59, 0x431e, { 0xbf, 0xf9, 0xe1, 0x1f, 0x2d, 0x62, 0x13, 0xda } };

QuicksyncDecoderPlugin::QuicksyncDecoderPlugin()
{
    m_usageWatcher.reset( new PluginUsageWatcher( QByteArray( QUICKSYNC_PLUGIN_ID ) ) );
    m_usageCalculator.reset( new PredefinedUsageCalculator( m_usageWatcher.get() ) );
}

quint32 QuicksyncDecoderPlugin::minSupportedVersion() const
{
    //TODO/IMPL
    return 1<<24;
}

//!Implementation of QnAbstractVideoDecoderPlugin::supportedCodecTypes
QList<CodecID> QuicksyncDecoderPlugin::supportedCodecTypes() const
{
    QList<CodecID> codecList;
    codecList.push_back( CODEC_ID_H264 );
    return codecList;
}

//!Implementation of QnAbstractVideoDecoderPlugin::isHardwareAccelerated
bool QuicksyncDecoderPlugin::isHardwareAccelerated() const
{
    return true;
}

//!Implementation of QnAbstractVideoDecoderPlugin::create
QnAbstractVideoDecoder* QuicksyncDecoderPlugin::create(
    CodecID codecID,
    const QnCompressedVideoDataPtr& data,
    const QGLContext* const glContext ) const
{
    if( codecID != CODEC_ID_H264 )
        return NULL;

    //TODO/IMPL filling stream parameters
    QList<MediaStreamParameter> mediaStreamParams;

    if( !m_usageCalculator->isEnoughHWResourcesForAnotherDecoder( mediaStreamParams ) )
        return NULL;

    std::auto_ptr<QuickSyncVideoDecoder> decoder( new QuickSyncVideoDecoder( data, m_usageWatcher.get() ) );
    if( !decoder->isHardwareAccelerationEnabled() )
        return NULL;

    return new VideoDecoderSwitcher( decoder.release(), data );
}

Q_EXPORT_PLUGIN2( quicksyncdecoderplugin, QuicksyncDecoderPlugin );
