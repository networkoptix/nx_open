// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtGui/QPainter>

#include <licensing/license_fwd.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/uuid.h>

static const char EXPORT_SIGN_MAGIC[] = "RhjrjLbkMxTujHI!";
static const nx::utils::QnCryptographicHash::Algorithm EXPORT_SIGN_METHOD = nx::utils::QnCryptographicHash::Md5;

class SPSUnit;
class PPSUnit;
class QnLicensePool;

class NX_VMS_COMMON_API QnSignHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnSignHelper(QObject* parent = nullptr);
    ~QnSignHelper();
    void setLogo(QPixmap logo);

    /** Get signature from encoded frame */
    QByteArray getSign(const AVFrame* frame, int signLen);

    /** Get hex signature from binary digest */
    static QByteArray getSignFromDigest(const QByteArray& digest);

    /** Get binary digest from hex sign */
    static QByteArray getDigestFromSign(const QByteArray& sign);

    /** Get signature from file end if it present*/
    static QByteArray loadSignatureFromFileEnd(const QString& filename);

    static QByteArray buildSignatureFileEnd(const QByteArray& signature);

    void setSign(const QByteArray& sign);
    void draw(QImage& img, bool drawText);
    void draw(QPainter& painter, const QSize& paintSize, bool drawText);
    //void drawTextLine(QPainter& painter, const QSize& paintSize,int lineNum, const QString& text);

    // TODO: #sivanov Remove magic const from the function.
    QFontMetrics updateFontSize(QPainter& painter, const QSize& paintSize);

    static void updateDigest(
        AVCodecParameters* avCodecParams,
        nx::utils::QnCryptographicHash& ctx,
        const quint8* data,
        int size);

    void setSignOpacity(float opacity, QColor color);

    /** Return initial signature as filler */
    static QByteArray getSignPattern(QnLicensePool* licensePool, const QnUuid& serverId);
    static char getSignPatternDelim();

    static QByteArray getSignMagic();

    /** Fix signature to make it precisely signSize() bytes length, filled with spaces. */
    static QByteArray addSignatureFiller(QByteArray source);

    void setVersionStr(const QString& value);
    void setHwIdStr(const QString& value);
    void setLicensedToStr(const QString& value);

private:
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
};
