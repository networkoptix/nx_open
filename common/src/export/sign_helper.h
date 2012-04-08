#ifndef __SIGN_FRAME_HELPER__
#define __SIGN_FRAME_HELPER__

#include <QByteArray>
#include "core/datapacket/mediadatapacket.h"
#include "utils/media/nalUnits.h"
#include <openssl/evp.h>

static const char EXPORT_SIGN_METHOD[] = "md5";
static const char EXPORT_SIGN_MAGIC[] = "RhjrjLbkMxTujHI!";

class QnSignHelper
{
public:
    void setLogo(QPixmap logo);
    QnCompressedVideoDataPtr createSgnatureFrame(AVCodecContext* srcCodec);
    QByteArray getSign(const AVFrame* frame, int signLen);
    void setSign(const QByteArray& sign);
    void draw(QImage& img, bool drawText);
    void draw(QPainter& painter, const QSize& paintSize, bool drawText);
    void drawTextLine(QPainter& painter, const QSize& paintSize,int lineNum, const QString& text);
    QFontMetrics updateFontSize(QPainter& painter, const QSize& paintSize);
    static void updateDigest(AVCodecContext* srcCodec, EVP_MD_CTX* mdctx, const quint8* data, int size);
private:
    void drawOnSignFrame(AVFrame* frame);
    void extractSpsPpsFromPrivData(const QByteArray& data, SPSUnit& sps, PPSUnit& pps, bool& spsReady, bool& ppsReady);
    void fillH264EncoderParams(const QByteArray& srcCodecExtraData, AVCodecContext* avctx, AVDictionary* &options);
    int correctX264Bitstream(const QByteArray& srcCodecExtraData, AVCodecContext* videoCodecCtx, quint8* videoBuf, int out_size, int videoBufSize);
    int correctNalPrefix(const QByteArray& srcCodecExtraData, quint8* videoBuf, int out_size, int videoBufSize);
private:
    QPixmap m_logo;
    QPixmap m_roundRectPixmap;
    QByteArray m_sign;
};

#endif // __SIGN_FRAME_HELPER__
