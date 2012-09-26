////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef VIDEODECODERSWITCHER_H
#define VIDEODECODERSWITCHER_H

#include "abstractdecoder.h"


//!Delegates calls to video decoder. Supports switching between soft and hw decoder
/*!
    Switching is done by external signal, so this class object itself does not decide when to switch
*/
class VideoDecoderSwitcher
:
    public QnAbstractVideoDecoder
{
public:
    /*!
        \param hwDecoder Decoder. Non-NULL! VideoDecoderSwitcher object takes ownership of \a hwDecoder
    */
    VideoDecoderSwitcher( QnAbstractVideoDecoder* hwDecoder );

    //!Implementation of QnAbstractVideoDecoder::GetPixelFormat
    virtual PixelFormat GetPixelFormat() const;
    //!Implementation of QnAbstractVideoDecoder::targetMemoryType
    virtual QnAbstractPictureData::PicStorageType targetMemoryType() const;
    //!Implementation of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame );
    //!Implementation of QnAbstractVideoDecoder::setLightCpuMode
    virtual void setLightCpuMode( DecodeMode val );
    //!Implementation of QnAbstractVideoDecoder::setMTDecoding
    virtual void setMTDecoding( bool value );
    //!Implementation of QnAbstractVideoDecoder::isMultiThreadedDecoding
    virtual bool isMultiThreadedDecoding() const;
    //!Implementation of QnAbstractVideoDecoder::isHardwareAccelerationEnabled
    bool isHardwareAccelerationEnabled() const;
    //!Implementation of QnAbstractVideoDecoder::getWidth
    virtual int getWidth() const;
    //!Implementation of QnAbstractVideoDecoder::getHeight
    virtual int getHeight() const;
    //!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const;
    //!Implementation of QnAbstractVideoDecoder::lastFrame
    virtual const AVFrame* lastFrame() const;
    //!Implementation of QnAbstractVideoDecoder::resetDecoder
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
    //!Implementation of QnAbstractVideoDecoder::setOutPictureSize
    virtual void setOutPictureSize( const QSize& outSize );

    //!Switches to ffmpeg decoder, destroys hardware decoder object
    void switchToSoftwareDecoding();

private:
    QnAbstractVideoDecoder* m_decoder;
    //!Used to initialize new decoder object in case of decoder switching
    QnCompressedVideoDataPtr m_mediaSequenceHeader;
};

#endif  //VIDEODECODERSWITCHER_H
