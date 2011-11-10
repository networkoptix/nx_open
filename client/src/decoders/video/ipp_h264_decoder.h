#ifndef ipp_decoder_h149
#define ipp_decoder_h149

#ifdef Q_OS_WIN
#include "abstractdecoder.h"

class IPPH264Decoder : public QnAbstractVideoDecoder
{
public:
    IPPH264Decoder();
    ~IPPH264Decoder();

    virtual bool decode(const QnCompressedVideoDataPtr data, CLVideoDecoderOutput* outFrame);


    void showMotion(bool) {}
    void setLightCpuMode(DecodeMode) {}

private:
    unsigned long m_decoder;
};

#endif

#endif //ipp_decoder_h149
