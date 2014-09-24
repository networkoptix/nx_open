#ifndef ipp_decoder_h149
#define ipp_decoder_h149

#ifdef Q_OS_WIN

#include "abstractdecoder.h"


class IPPH264Decoder : public QnAbstractVideoDecoder
{
public:
    IPPH264Decoder();
    ~IPPH264Decoder();

    virtual bool decode(const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame) override;
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const override;
    virtual void resetDecoder( const QnConstCompressedVideoDataPtr& data ) override;
    virtual void setOutPictureSize( const QSize& outSize ) override;
    void setLightCpuMode(DecodeMode) {}

    virtual QnAbstractPictureDataRef::PicStorageType targetMemoryType() const override { return QnAbstractPictureDataRef::pstSysMemPic; }
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    virtual unsigned int getDecoderCaps() const override;
    //!Implementation of QnAbstractVideoDecoder::setSpeed
    virtual void setSpeed( float newValue ) override;

private:
    unsigned long m_decoder;
};

#endif

#endif //ipp_decoder_h149
