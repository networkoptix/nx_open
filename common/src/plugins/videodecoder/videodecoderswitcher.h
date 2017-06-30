////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef VIDEODECODERSWITCHER_H
#define VIDEODECODERSWITCHER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>

#include <nx/utils/thread/mutex.h>

#include "abstract_decoder_event_receiver.h"
#include "../../decoders/video/abstract_video_decoder.h"


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
    virtual AVPixelFormat GetPixelFormat() const override;
    //!Implementation of QnAbstractVideoDecoder::targetMemoryType
    virtual QnAbstractPictureDataRef::PicStorageType targetMemoryType() const override;
    //!Implementation of QnAbstractVideoDecoder::decode
    virtual bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame ) override;
    //!Implementation of QnAbstractVideoDecoder::setLightCpuMode
    virtual void setLightCpuMode( DecodeMode val ) override;
    //!Implementation of QnAbstractVideoDecoder::setMTDecoding
    virtual void setMTDecoding( bool value ) override;
    //!Implementation of QnAbstractVideoDecoder::isMultiThreadedDecoding
    virtual bool isMultiThreadedDecoding() const override;
    //!Implementation of QnAbstractVideoDecoder::isHardwareAccelerationEnabled
    bool isHardwareAccelerationEnabled() const override;
    //!Implementation of QnAbstractVideoDecoder::getWidth
    virtual int getWidth() const override;
    //!Implementation of QnAbstractVideoDecoder::getHeight
    virtual int getHeight() const override;
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const override;
    //!Implementation of QnAbstractVideoDecoder::getSampleAspectRatio
    virtual double getSampleAspectRatio() const override;
    //!Implementation of QnAbstractVideoDecoder::lastFrame
    virtual const AVFrame* lastFrame() const override;
    //!Implementation of QnAbstractVideoDecoder::resetDecoder
    virtual void resetDecoder( const QnConstCompressedVideoDataPtr& data ) override;
    //!Implementation of QnAbstractVideoDecoder::setOutPictureSize
    virtual void setOutPictureSize( const QSize& outSize ) override;
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    virtual unsigned int getDecoderCaps() const override;
    //!Implementation of QnAbstractVideoDecoder::setSpeed
    virtual void setSpeed( float newValue ) override;

    //!Implementation of AbstractDecoderEventReceiver::streamParamsChanged
    virtual DecoderBehaviour streamParamsChanged(
        QnAbstractVideoDecoder* decoder,
        const nx::utils::stree::AbstractResourceReader& newStreamParams ) override;

    /*!
        Takes ownership of \a hwDecoder
    */
    void setHWDecoder( QnAbstractVideoDecoder* hwDecoder );
    //!Switches to ffmpeg decoder, destroys hardware decoder object
    void switchToSoftwareDecoding();

private:
    std::unique_ptr<QnAbstractVideoDecoder> m_decoder;
    //!Used to initialize new decoder object in case of decoder switching
    QnCompressedVideoDataPtr m_mediaSequenceHeader;
    const QnAbstractVideoDecoderPlugin& m_decoderFactory;
    bool m_switchToSWDecoding;
    mutable QnMutex m_mutex;
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //VIDEODECODERSWITCHER_H
