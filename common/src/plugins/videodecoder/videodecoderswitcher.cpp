////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "videodecoderswitcher.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/mutex.h>

#include "decoders/abstractvideodecoderplugin.h"
#include "decoders/video/ffmpeg.h"


VideoDecoderSwitcher::VideoDecoderSwitcher(
    QnAbstractVideoDecoder* hwDecoder,
    const QnCompressedVideoDataPtr& sequenceHeader,
    const QnAbstractVideoDecoderPlugin& decoderFactory )
:
    m_decoder( hwDecoder ),
    m_mediaSequenceHeader( sequenceHeader ),
    m_decoderFactory( decoderFactory ),
    m_switchToSWDecoding( false )
{
}

//!Implementation of QnAbstractVideoDecoder::GetPixelFormat
PixelFormat VideoDecoderSwitcher::GetPixelFormat() const
{
    return m_decoder->GetPixelFormat();
}

//!Implementation of QnAbstractVideoDecoder::targetMemoryType
QnAbstractPictureDataRef::PicStorageType VideoDecoderSwitcher::targetMemoryType() const
{
    return m_decoder->targetMemoryType();
}

//!Implementation of QnAbstractVideoDecoder::decode
bool VideoDecoderSwitcher::decode( const QnConstCompressedVideoDataPtr data, QSharedPointer<CLVideoDecoderOutput>* const outFrame )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    bool res = false;
    for( int i = 0; i < 2; ++i )
    {
        //performing following condition check before and after m_decoder->decode call
        if( m_switchToSWDecoding && data && (data->flags & AV_PKT_FLAG_KEY) )
        {
            m_decoder.reset( new CLFFmpegVideoDecoder( data->compressionType, data, true, NULL ) ); //TODO/IMPL 3rd and 4th params
            m_switchToSWDecoding = false;
        }

        if( i == 1 )
            break;

        res = m_decoder->decode( data, outFrame );
    }

    return res;
}

//!Implementation of QnAbstractVideoDecoder::setLightCpuMode
void VideoDecoderSwitcher::setLightCpuMode( DecodeMode val )
{
    return m_decoder->setLightCpuMode( val );
}

//!Implementation of QnAbstractVideoDecoder::setMTDecoding
void VideoDecoderSwitcher::setMTDecoding( bool value )
{
    return m_decoder->setMTDecoding( value );
}

//!Implementation of QnAbstractVideoDecoder::isMultiThreadedDecoding
bool VideoDecoderSwitcher::isMultiThreadedDecoding() const
{
    return m_decoder->isMultiThreadedDecoding();
}

//!Implementation of QnAbstractVideoDecoder::isHardwareAccelerationEnabled
bool VideoDecoderSwitcher::isHardwareAccelerationEnabled() const
{
    return m_decoder->isHardwareAccelerationEnabled();
}

//!Implementation of QnAbstractVideoDecoder::getWidth
int VideoDecoderSwitcher::getWidth() const
{
    return m_decoder->getWidth();
}

//!Implementation of QnAbstractVideoDecoder::getHeight
int VideoDecoderSwitcher::getHeight() const
{
    return m_decoder->getHeight();
}

QSize VideoDecoderSwitcher::getOriginalPictureSize() const
{
    return m_decoder->getOriginalPictureSize();
}

//!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
double VideoDecoderSwitcher::getSampleAspectRatio() const
{
    return m_decoder->getSampleAspectRatio();
}

//!Implementation of QnAbstractVideoDecoder::lastFrame
const AVFrame* VideoDecoderSwitcher::lastFrame() const
{
    return m_decoder->lastFrame();
}

//!Implementation of QnAbstractVideoDecoder::resetDecoder
void VideoDecoderSwitcher::resetDecoder( QnConstCompressedVideoDataPtr data )
{
    return m_decoder->resetDecoder( data );
}

//!Implementation of QnAbstractVideoDecoder::setOutPictureSize
void VideoDecoderSwitcher::setOutPictureSize( const QSize& outSize )
{
    return m_decoder->setOutPictureSize( outSize );
}

unsigned int VideoDecoderSwitcher::getDecoderCaps() const
{
    return m_decoder->getDecoderCaps();
}

void VideoDecoderSwitcher::setSpeed( float newValue )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    if( m_decoder.get() )
        m_decoder->setSpeed( newValue );
}

//!Implementation of AbstractDecoderManagerCallback::streamParamsChanged
AbstractDecoderEventReceiver::DecoderBehaviour VideoDecoderSwitcher::streamParamsChanged(
    QnAbstractVideoDecoder* /*decoder*/,
    const stree::AbstractResourceReader& newStreamParams )
{
    //NOTE no need to lock mutex, since this method can only be called from decode, so mutex is already locked

    //checking, whether new stream is supported
    if( m_decoderFactory.isStreamSupported( newStreamParams ) )
        return AbstractDecoderEventReceiver::dbContinueHardWork;

    //marking HW decoder for release
    m_switchToSWDecoding = true;
    return AbstractDecoderEventReceiver::dbStop;
}

void VideoDecoderSwitcher::setHWDecoder( QnAbstractVideoDecoder* hwDecoder )
{
    m_decoder.reset( hwDecoder );
}

//!Switches to ffmpeg decoder, destroys hardware decoder object
void VideoDecoderSwitcher::switchToSoftwareDecoding()
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    m_switchToSWDecoding = true;
}

#endif // ENABLE_DATA_PROVIDERS
