#ifndef ipp_decoder_h149
#define ipp_decoder_h149

#ifdef Q_OS_WIN

#include "abstractdecoder.h"


class IPPH264Decoder : public QnAbstractVideoDecoder
{
public:
    IPPH264Decoder();
    ~IPPH264Decoder();

    virtual bool decode(const QnCompressedVideoDataPtr data, QSharedPointer<CLVideoDecoderOutput>* const outFrame);
    //!Implementation of QnAbstractVideoDecoder::getOriginalPictureSize
    virtual QSize getOriginalPictureSize() const;
    virtual void resetDecoder( QnCompressedVideoDataPtr data );
    virtual void setOutPictureSize( const QSize& outSize );
    void setLightCpuMode(DecodeMode) {}

    QnAbstractPictureDataRef::PicStorageType targetMemoryType() const { return QnAbstractPictureDataRef::pstSysMemPic; }
    //!Implementation of QnAbstractVideoDecoder::getDecoderCaps
    virtual unsigned int getDecoderCaps() const;

private:
    unsigned long m_decoder;
};

#endif

#endif //ipp_decoder_h149
