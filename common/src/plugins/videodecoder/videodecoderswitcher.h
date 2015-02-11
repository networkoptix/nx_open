////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef VIDEODECODERSWITCHER_H
#define VIDEODECODERSWITCHER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>

#include <utils/common/mutex.h>

#include "abstract_decoder_event_receiver.h"
#include "../../decoders/video/abstractdecoder.h"


class QnAbstractVideoDecoderPlugin;

//!Delegates calls to video decoder. Supports switching between soft and hw decoder
/*!
    Can switch to SW decoding on stream params change or on external signal (\a switchToSoftwareDecoding() method call)
    \note switchToSoftwareDecoding is allowed to be called from thread, different from decoding thread
*/
class VideoDecoderSwitcher
:
    public QnAbstractVideoDecoder,
    public AbstractDecoderEventReceiver
{
public:
    /*!
        \param hwDecoder Decoder. Can be specified later with \a setHWDecoder call. VideoDecoderSwitcher object takes ownership of \a hwDecoder
    */
    VideoDecoderSwitcher(
        QnAbstractVideoDecoder* hwDecoder,
        const QnCompressedVideoDataPtr& sequenceHeader,
        const QnAbstractVideoDecoderPlugin& decoderFactory );

    //!Implementation of QnAbstractVideoDecoder::GetPixelFormat
    virtual PixelFormat GetPixelFormat() const;
    //!Implementation of QnAbstractVideoDecoder::targetMemoryType
    virtual QnAbstractPictureDataRef::PicStorageType targetMemoryType() const;
    //!Implementation of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnConstCompressedVideoDataPtr data, QSharedPointer<CLVideoDecoderOutput>* const outFrame );
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
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const;
    //!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const;
    //!Implementation of QnAbstractVideoDecoder::lastFrame
    virtual const AVFrame* lastFrame() const;
    //!Implementation of QnAbstractVideoDecoder::resetDecoder
    virtual void resetDecoder( QnConstCompressedVideoDataPtr data );
    //!Implementation of QnAbstractVideoDecoder::setOutPictureSize
    virtual void setOutPictureSize( const QSize& outSize );
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    virtual unsigned int getDecoderCaps() const;
    //!Implementation of QnAbstractVideoDecoder::setSpeed
    virtual void setSpeed( float newValue );

    //!Implementation of AbstractDecoderEventReceiver::streamParamsChanged
    virtual DecoderBehaviour streamParamsChanged(
        QnAbstractVideoDecoder* decoder,
        const stree::AbstractResourceReader& newStreamParams );

    /*!
        Takes ownership of \a hwDecoder
    */
    void setHWDecoder( QnAbstractVideoDecoder* hwDecoder );
    //!Switches to ffmpeg decoder, destroys hardware decoder object
    void switchToSoftwareDecoding();

private:
    std::auto_ptr<QnAbstractVideoDecoder> m_decoder;
    //!Used to initialize new decoder object in case of decoder switching
    QnCompressedVideoDataPtr m_mediaSequenceHeader;
    const QnAbstractVideoDecoderPlugin& m_decoderFactory;
    bool m_switchToSWDecoding;
    mutable QnMutex m_mutex;
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //VIDEODECODERSWITCHER_H
