#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtGui/QPainter>

#include <common/common_module_aware.h>

#include <licensing/license_fwd.h>

#include <nx/utils/cryptographic_hash.h>

#include <nx/streaming/video_data_packet.h>

static const char EXPORT_SIGN_MAGIC[] = "RhjrjLbkMxTujHI!";
static const nx::utils::QnCryptographicHash::Algorithm EXPORT_SIGN_METHOD = nx::utils::QnCryptographicHash::Md5;

class SPSUnit;
class PPSUnit;

class QnSignHelper: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnSignHelper(QnCommonModule* commonModule, QObject* parent = nullptr);
    ~QnSignHelper();
    void setLogo(QPixmap logo);
    QnCompressedVideoDataPtr createSignatureFrame(AVCodecContext* srcCodec, QnCompressedVideoDataPtr iFrame);

    /** Get signature from encoded frame */
    QByteArray getSign(const AVFrame* frame, int signLen);

    /** Get hex signature from binary digest */
    static QByteArray getSignFromDigest(const QByteArray& digest);

    /** Get binary digest from hex sign */
    static QByteArray getDigestFromSign(const QByteArray& sign);

    void setSign(const QByteArray& sign);
    void draw(QImage& img, bool drawText);
    void draw(QPainter& painter, const QSize& paintSize, bool drawText);
    //void drawTextLine(QPainter& painter, const QSize& paintSize,int lineNum, const QString& text);

    // TODO: #Elric remove magic const from the function
    QFontMetrics updateFontSize(QPainter& painter, const QSize& paintSize);
    static void updateDigest(AVCodecContext* srcCodec, nx::utils::QnCryptographicHash &ctx, const quint8* data, int size);
    static void updateDigest(const QnConstMediaContextPtr& context, nx::utils::QnCryptographicHash &ctx, const quint8* data, int size);
    void setSignOpacity(float opacity, QColor color);

    /** Return initial signature as filler */
    static QByteArray getSignPattern(QnLicensePool* licensePool);
    static char getSignPatternDelim();

    static QByteArray getSignMagic();

    /** Fix signature to make it precisely signSize() bytes length, filled with spaces. */
    static QByteArray makeSignature(QByteArray source);

    void setVersionStr(const QString& value);
    void setHwIdStr(const QString& value);
    void setLicensedToStr(const QString& value);
private:
    void drawOnSignFrame(AVFrame* frame);
    void extractSpsPpsFromPrivData(const quint8* buffer, int bufferSize, SPSUnit& sps, PPSUnit& pps, bool& spsReady, bool& ppsReady);
    QString fillH264EncoderParams(const QByteArray& srcCodecExtraData, QnCompressedVideoDataPtr iFrame, AVCodecContext* avctx);
    int correctX264Bitstream(const QByteArray& srcCodecExtraData, QnCompressedVideoDataPtr iFrame, AVCodecContext* videoCodecCtx, quint8* videoBuf, int out_size, int videoBufSize);
    int correctNalPrefix(const QByteArray& srcCodecExtraData, quint8* videoBuf, int out_size, int videoBufSize);
    int runX264Process(AVFrame* frame, QString optionStr, quint8* rezBuffer);
    int removeH264SeiMessage(quint8* buffer, int size);
    static void doUpdateDigestNoCodec(nx::utils::QnCryptographicHash &ctx, const quint8* data, int size);
    static void doUpdateDigest(AVCodecID codecId, const quint8* extradata, int extradataSize, nx::utils::QnCryptographicHash &ctx, const quint8* data, int size);

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
    QString m_versionStr;
    QString m_hwIdStr;
    QString m_licensedToStr;
    AVPacket* m_outPacket;

    QnLicenseValidator* m_licenseValidator = nullptr;
};
