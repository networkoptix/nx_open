#ifndef __SIGN_FRAME_HELPER__
#define __SIGN_FRAME_HELPER__

#include <QtCore/QByteArray>

#include <utils/common/cryptographic_hash.h>
#include <utils/media/nalUnits.h>

#include <core/datapacket/media_data_packet.h>

static const char EXPORT_SIGN_MAGIC[] = "RhjrjLbkMxTujHI!";
static const QnCryptographicHash::Algorithm EXPORT_SIGN_METHOD = QnCryptographicHash::Md5;

class QnSignHelper
{
    Q_DECLARE_TR_FUNCTIONS(QnSignHelper)
public:
    QnSignHelper();
    void setLogo(QPixmap logo);
    QnCompressedVideoDataPtr createSgnatureFrame(AVCodecContext* srcCodec, QnCompressedVideoDataPtr iFrame);
    QByteArray getSign(const AVFrame* frame, int signLen);
    void setSign(const QByteArray& sign);
    void draw(QImage& img, bool drawText);
    void draw(QPainter& painter, const QSize& paintSize, bool drawText);
    //void drawTextLine(QPainter& painter, const QSize& paintSize,int lineNum, const QString& text);
    
    /** TODO: remove magic const from the function */
    QFontMetrics updateFontSize(QPainter& painter, const QSize& paintSize);
    static void updateDigest(AVCodecContext* srcCodec, QnCryptographicHash &ctx, const quint8* data, int size);
    void setSignOpacity(float opacity, QColor color);

private:
    void drawOnSignFrame(AVFrame* frame);
    void extractSpsPpsFromPrivData(const quint8* buffer, int bufferSize, SPSUnit& sps, PPSUnit& pps, bool& spsReady, bool& ppsReady);
    QString fillH264EncoderParams(const QByteArray& srcCodecExtraData, QnCompressedVideoDataPtr iFrame, AVCodecContext* avctx);
    int correctX264Bitstream(const QByteArray& srcCodecExtraData, QnCompressedVideoDataPtr iFrame, AVCodecContext* videoCodecCtx, quint8* videoBuf, int out_size, int videoBufSize);
    int correctNalPrefix(const QByteArray& srcCodecExtraData, quint8* videoBuf, int out_size, int videoBufSize);
    int runX264Process(AVFrame* frame, QString optionStr, quint8* rezBuffer);
    int removeH264SeiMessage(quint8* buffer, int size);

private:
    QPixmap m_logo;
    QPixmap m_roundRectPixmap;
    QByteArray m_sign;

    float m_opacity;
    QColor m_signBackground;

    QSize m_lastPaintSize;
    QFont m_cachedFont;
    QFontMetrics m_cachedMetric;
    QPixmap m_backgroundPixmap;
};

#endif // __SIGN_FRAME_HELPER__
