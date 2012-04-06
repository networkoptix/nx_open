#ifndef __SIGN_FRAME_HELPER__
#define __SIGN_FRAME_HELPER__

#include <QByteArray>
#include "core/datapacket/mediadatapacket.h"
#include "utils/media/nalUnits.h"

class QnSignHelper
{
public:
    void setLogo(QPixmap logo);
    QnCompressedVideoDataPtr createSgnatureFrame(AVCodecContext* srcCodec, const QByteArray& hash);
private:
    void drawOnSignFrame(AVFrame* frame, const QByteArray& sign);
    void extractSpsPpsFromPrivData(const QByteArray& data, SPSUnit& sps, PPSUnit& pps, bool& spsReady, bool& ppsReady);
    void fillH264EncoderParams(const QByteArray& srcCodecExtraData, AVCodecContext* avctx, AVDictionary* &options);
    int correctX264Bitstream(const QByteArray& srcCodecExtraData, AVCodecContext* videoCodecCtx, quint8* videoBuf, int out_size, int videoBufSize);
    int correctNalPrefix(const QByteArray& srcCodecExtraData, quint8* videoBuf, int out_size, int videoBufSize);
private:
    QPixmap m_logo;
};

#endif // __SIGN_FRAME_HELPER__
