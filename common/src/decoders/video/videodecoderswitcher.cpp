////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "videodecoderswitcher.h"


VideoDecoderSwitcher::VideoDecoderSwitcher( QnAbstractVideoDecoder* hwDecoder )
:
    m_decoder( hwDecoder )
{
}

//!Implementation of QnAbstractVideoDecoder::GetPixelFormat
PixelFormat VideoDecoderSwitcher::GetPixelFormat() const
{
    return m_decoder->GetPixelFormat();
}

//!Implementation of QnAbstractVideoDecoder::targetMemoryType
QnAbstractPictureData::PicStorageType VideoDecoderSwitcher::targetMemoryType() const
{
    return m_decoder->targetMemoryType();
}

//!Implementation of QnAbstractVideoDecoder::decode
bool VideoDecoderSwitcher::decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame )
{
    return m_decoder->decode( data, outFrame );
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
void VideoDecoderSwitcher::resetDecoder( QnCompressedVideoDataPtr data )
{
    return m_decoder->resetDecoder( data );
}

//!Implementation of QnAbstractVideoDecoder::setOutPictureSize
void VideoDecoderSwitcher::setOutPictureSize( const QSize& outSize )
{
    return m_decoder->setOutPictureSize( outSize );
}

//!Switches to ffmpeg decoder, destroys hardware decoder object
void VideoDecoderSwitcher::switchToSoftwareDecoding()
{
    //TODO/IMPL
}
