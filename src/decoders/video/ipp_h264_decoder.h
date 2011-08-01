#ifndef ipp_decoder_h149
#define ipp_decoder_h149

#ifdef Q_OS_WIN
#include "abstractdecoder.h"

class IPPH264Decoder : public CLAbstractVideoDecoder
{
public:
    IPPH264Decoder();
    ~IPPH264Decoder();

    bool decode(CLVideoData &params);

    void showMotion(bool) {}
    void setLightCpuMode(bool) {}

private:
    unsigned long m_decoder;
};

#endif

#endif //ipp_decoder_h149
