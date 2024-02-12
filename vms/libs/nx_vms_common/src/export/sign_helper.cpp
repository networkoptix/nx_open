// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sign_helper.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#ifdef WIN32
#define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif
};

#include <QtCore/QProcess>
#include <QtCore/QTemporaryFile>

#include <licensing/license.h>
#include <nx/codec/nal_units.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/util.h>

using namespace nx::utils;

namespace {

constexpr char kEndFileSignatureGuid[] = "29b5406f33174153aa5b3a63938507fe";

static const int text_x_offs = 16;
static const int text_y_offs = 16;
char SIGN_TEXT_DELIMITER('\\');
char SIGN_TEXT_DELIMITER_REPLACED('/');
static constexpr int kSignatureSize = 512;

QByteArray INITIAL_SIGNATURE_MAGIC("BCDC833CB81C47bc83B37ECD87FD5217"); // initial MD5 hash
QByteArray SIGNATURE_XOR_MAGIC = QByteArray::fromHex("B80466320F15448096F7CEE3379EEF78");

} // namespace


int getSquareSize(int width, int height, int signBits)
{

    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;
    int SQUARE_SIZE = qMin((height/2-height/32)/rowCnt, (width-width/32)/colCnt);
    SQUARE_SIZE = qPower2Floor(SQUARE_SIZE, 2);
    return SQUARE_SIZE;
}

float getAvgColor(const AVFrame* frame, int plane, const QRect& rect)
{
    quint8* curPtr = frame->data[plane] + rect.top()*frame->linesize[plane] + rect.left();
    float sum = 0;
    for (int y = 0; y < rect.height(); ++y)
    {
        for (int x = 0; x < rect.width(); ++x)
        {
            sum += curPtr[x];
        }
        curPtr += frame->linesize[plane];
    }
    return sum / (rect.width() * rect.height());
}

QnSignHelper::QnSignHelper(QObject* parent):
    base_type(parent),
    m_cachedMetric(QFont()),
    m_outPacket(av_packet_alloc())
{
    m_opacity = 1.0;
    m_signBackground = Qt::white;

    m_versionStr = qApp->applicationName()
        .append(" v")
        .append(QCoreApplication::applicationVersion());
}

QnSignHelper::~QnSignHelper()
{
    av_packet_free(&m_outPacket);
}

void QnSignHelper::updateDigest(AVCodecParameters* avCodecParams, QnCryptographicHash &ctx, const quint8* data, int size)
{
    if (!avCodecParams)
        doUpdateDigestNoCodec(ctx, data, size);
    else
        doUpdateDigest(avCodecParams->codec_id, avCodecParams->extradata, avCodecParams->extradata_size, ctx, data, size);
}

void QnSignHelper::doUpdateDigestNoCodec(QnCryptographicHash &ctx, const quint8* data, int size)
{
    ctx.addData((const char*) data, size);
}

void QnSignHelper::doUpdateDigest(AVCodecID codecId, const quint8* extradata, int extradataSize, QnCryptographicHash &ctx, const quint8* data, int size)
{
    if (codecId != AV_CODEC_ID_H264)
    {
        doUpdateDigestNoCodec(ctx, data, size);
        return;
    }

    // skip nal prefixes (sometimes it is 00 00 01 code, sometimes unitLen)
    const quint8* dataEnd = data + size;

    if (extradataSize >= 7 && extradata[0] == 1)
    {
        // prefix is unit len
        int reqUnitSize = (extradata[4] & 0x03) + 1;

        // Data pointer can be completely empty sometimes.
        if (const quint8* curNal = data)
        {
            while (curNal < dataEnd - reqUnitSize)
            {
                uint32_t curSize = 0;
                for (int i = 0; i < reqUnitSize; ++i)
                    curSize = (curSize << 8) + curNal[i];
                curNal += reqUnitSize;
                curSize = qMin(curSize, (uint32_t) (dataEnd - curNal));
                ctx.addData((const char*) curNal, (int) curSize);

                curNal += curSize;
            }
        }
    }
    else
    {
        // prefix is 00 00 01 code
        if (const quint8* curNal = NALUnit::findNextNAL(data, dataEnd))
        {
            while (curNal < dataEnd)
            {
                const quint8* nextNal = NALUnit::findNALWithStartCode(curNal, dataEnd, true);
                ctx.addData((const char*)curNal, nextNal - curNal);

                curNal = NALUnit::findNextNAL(nextNal, dataEnd);
            }
        }
    }
}

QByteArray QnSignHelper::getSign(const AVFrame* frame, int signLen)
{
    static const int COLOR_THRESHOLD = 38;

    int signBits = signLen*8;
    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;
    int SQUARE_SIZE = getSquareSize(frame->width, frame->height, signBits);
    int drawWidth = SQUARE_SIZE * colCnt;
    int drawheight = SQUARE_SIZE * rowCnt;

    int drawLeft = (frame->width - drawWidth)/2;
    int drawTop = frame->height/2 + (frame->height/2-drawheight)/2;

    int cOffs = SQUARE_SIZE/8;
    quint8 signArray[256/8];
    nx::utils::BitStreamWriter writer;
    writer.setBuffer(signArray, signArray + sizeof(signArray));
    for (int y = 0; y < rowCnt; ++y)
    {
        for (int x = 0; x < colCnt; ++x)
        {
            QRect checkRect(x*SQUARE_SIZE+cOffs, y*SQUARE_SIZE+cOffs, SQUARE_SIZE-cOffs*2, SQUARE_SIZE-cOffs*2);
            checkRect.translate(drawLeft, drawTop);

            float avgColor = getAvgColor(frame, 0, checkRect);
            if (avgColor <= COLOR_THRESHOLD)
                writer.putBit(1);
            else if (avgColor >= (255-COLOR_THRESHOLD))
                writer.putBit(0);
            else
                return QByteArray(); // invalid data

            // check colors plane
            const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) frame->format);
            for (int i = 0; i < descr->log2_chroma_w; ++i)
                checkRect = QRect(checkRect.left()/2, checkRect.top(), checkRect.width()/2, checkRect.height());
            for (int i = 0; i < descr->log2_chroma_h; ++i)
                checkRect = QRect(checkRect.left(), checkRect.top()/2, checkRect.width(), checkRect.height()/2);
            for (int i = 1; i < descr->nb_components; ++i) {
                float avgColor = getAvgColor(frame, i, checkRect);
                if (qAbs(avgColor - 0x80) > COLOR_THRESHOLD/2)
                    return QByteArray(); // invalid data
            }
        }
    }
    return QByteArray((const char*)signArray, signLen);
}

QByteArray QnSignHelper::getSignFromDigest(const QByteArray& digest)
{
    QByteArray result = digest;
    for (int i = 0; i < result.size(); ++i)
        result.data()[i] ^= SIGNATURE_XOR_MAGIC[i % SIGNATURE_XOR_MAGIC.size()];
    return result.toHex();
}

QByteArray QnSignHelper::getDigestFromSign(const QByteArray& sign)
{
    QByteArray result = QByteArray::fromHex(sign);
    for (int i = 0; i < result.size(); ++i)
        result.data()[i] ^= SIGNATURE_XOR_MAGIC[i % SIGNATURE_XOR_MAGIC.size()];
    return result;
}

QFontMetrics QnSignHelper::updateFontSize(QPainter& painter, const QSize& paintSize)
{
    if (m_lastPaintSize == paintSize)
    {
        painter.setFont(m_cachedFont);
        return m_cachedMetric;
    }

    QFont font;
    font.setPointSize(1);
    QFontMetrics metric(font);
    for (int i = 0; i < 100; ++i)
    {
        metric = QFontMetrics(font);
        int width = metric.horizontalAdvance(m_versionStr);
        int height = metric.height();
        if (width >= paintSize.width()/2 || height >= (paintSize.height()/2-text_y_offs) / 4)
            break;
        font.setPointSize(font.pointSize() + 1);
    }
    painter.setFont(font);

    m_lastPaintSize = paintSize;
    m_cachedFont = font;
    m_cachedMetric = metric;
    return metric;
}

void QnSignHelper::setSignOpacity(float opacity, QColor color)
{
    m_opacity = opacity;
    //if (m_signBackground != color)
    //    m_roundRectPixmap = QPixmap();
    m_signBackground = color;
}

QColor blendColor(QColor color, float opacity, QColor backgroundColor)
{
    int r = backgroundColor.red()*(1-opacity) + color.red()*opacity;
    int g = backgroundColor.green()*(1-opacity) + color.green()*opacity;
    int b = backgroundColor.blue()*(1-opacity) + color.blue()*opacity;
    return QColor(r,g,b);
}

void QnSignHelper::draw(QPainter& painter, const QSize& paintSize, bool drawText)
{
    // We should restore this transform at the exit.
    const auto initialTransform = painter.transform();

    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    if (m_backgroundPixmap.size() != paintSize)
    {
        m_backgroundPixmap = QPixmap(paintSize);
        QPainter p3(&m_backgroundPixmap);
        p3.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        p3.fillRect(0,0, paintSize.width(), paintSize.height()/2, Qt::white);
        p3.fillRect(0,paintSize.height()/2, paintSize.width(), paintSize.height()/2, Qt::gray);

        if (m_logo.width() > 0) {
            QPixmap logo = m_logo.scaledToWidth(paintSize.width()/8, Qt::SmoothTransformation);
            p3.drawPixmap(QPoint(paintSize.width() - logo.width() - text_x_offs, paintSize.height()/2 - logo.height() - text_y_offs), logo);
        }
    }
    painter.drawPixmap(0,0, m_backgroundPixmap);

    if (drawText)
    {
        QFontMetrics metric = updateFontSize(painter, paintSize);

        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()), m_versionStr);

        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*2), tr("Hardware ID: %1").arg(m_hwIdStr));
        if (!m_licensedToStr.isEmpty())
            painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*3), tr("Licensed To: %1").arg(m_licensedToStr));
        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*4), tr("Watermark: %1").arg(QLatin1String(m_sign.toHex())));
    }

    // draw Hash
    int signBits = m_sign.length()*8;
    int rowCnt = signBits / 16;
    int colCnt = signBits ? signBits / rowCnt : 0;

    int SQUARE_SIZE = signBits ? getSquareSize(paintSize.width(), paintSize.height(), signBits) : 0;


    int drawWidth = SQUARE_SIZE * colCnt;
    int drawheight = SQUARE_SIZE * rowCnt;
    painter.translate((paintSize.width() - drawWidth)/2, paintSize.height()/2 + (paintSize.height()/2-drawheight)/2);


    QColor signBkColor = blendColor(m_signBackground, m_opacity, Qt::black);
    painter.fillRect(0, 0, SQUARE_SIZE*colCnt, SQUARE_SIZE*rowCnt, signBkColor);

    int cOffs = SQUARE_SIZE/16;
    m_roundRectPixmap = QPixmap(SQUARE_SIZE, SQUARE_SIZE);
    QPainter tmpPainter(&m_roundRectPixmap);
    tmpPainter.fillRect(0,0, SQUARE_SIZE, SQUARE_SIZE, signBkColor);

    tmpPainter.setBrush(Qt::black);
    tmpPainter.setPen(QColor(signBkColor.red()/2,signBkColor.green()/2,signBkColor.blue()/2));
    tmpPainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    tmpPainter.drawRoundedRect(QRect(cOffs, cOffs, SQUARE_SIZE-cOffs*2, SQUARE_SIZE-cOffs*2), SQUARE_SIZE/4, SQUARE_SIZE/4);

    for (int y = 0; y < rowCnt; ++y)
    {
        for (int x = 0; x < colCnt; ++x)
        {
            int bitNum = y*colCnt + x;
            unsigned char b = *((unsigned char*)m_sign.data() + bitNum/8 );
            bool isBit = b & (128 >> (bitNum&7));
            if (isBit)
                painter.drawPixmap(x*SQUARE_SIZE, y*SQUARE_SIZE, m_roundRectPixmap);
        }
    }

    painter.setTransform(initialTransform);
}

void QnSignHelper::draw(QImage& img, bool drawText)
{
    QPainter painter(&img);
    draw(painter, img.size(), drawText);
    //img.save("c:/test.bmp");
}

void QnSignHelper::setLogo(QPixmap logo)
{
    m_logo = logo;
}

void QnSignHelper::setSign(const QByteArray& sign)
{
    m_sign = sign;
}

QByteArray QnSignHelper::getSignMagic()
{
    return INITIAL_SIGNATURE_MAGIC;
}

QByteArray QnSignHelper::addSignatureFiller(QByteArray source)
{
    while (source.size() < kSignatureSize)
        source.append(" ");
    return source.mid(0, kSignatureSize);
}

char QnSignHelper::getSignPatternDelim()
{
    return SIGN_TEXT_DELIMITER;
}

QByteArray QnSignHelper::getSignPattern(QnLicensePool* licensePool, const nx::Uuid& serverId)
{
    QByteArray result;
    result.append(INITIAL_SIGNATURE_MAGIC);
    result.append(SIGN_TEXT_DELIMITER);

    result.append(qApp->applicationName().toUtf8()).append(" v").append(QCoreApplication::applicationVersion().toUtf8()).append(SIGN_TEXT_DELIMITER);

    QString hid = licensePool->currentHardwareId(serverId);
    if (hid.isEmpty())
        hid = tr("Unknown");
    result.append(hid.toUtf8()).append(SIGN_TEXT_DELIMITER);

    QList<QnLicensePtr> list = licensePool->getLicenses();
    QString licenseName(tr("FREE License"));
    for (const QnLicensePtr& license: list)
    {
        if (license->name() != QLatin1String("FREE"))
            licenseName = license->name();
    }
    QByteArray bLicName = licenseName.toUtf8().replace(SIGN_TEXT_DELIMITER, SIGN_TEXT_DELIMITER_REPLACED);
    result.append(bLicName).append(SIGN_TEXT_DELIMITER);
    return result;
}

void QnSignHelper::setVersionStr(const QString& value)
{
    m_versionStr = value;
}

void QnSignHelper::setHwIdStr(const QString& value)
{
    m_hwIdStr = value;
}

void QnSignHelper::setLicensedToStr(const QString& value)
{
    m_licensedToStr = value;
}

QByteArray QnSignHelper::buildSignatureFileEnd(const QByteArray& signature)
{
    QByteArray result;
    result.append(kEndFileSignatureGuid, sizeof(kEndFileSignatureGuid));
    result.append(signature);
    return result;
}

QByteArray QnSignHelper::loadSignatureFromFileEnd(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly) || file.size() < kSignatureSize)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to open file [%1] to load signature", filename);
        return QByteArray();
    }

    if (!file.seek(file.size() - kSignatureSize))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to seek on file [%1] to load signature", filename);
        return QByteArray();
    }

    const auto data = file.read(kSignatureSize);
    int index = data.indexOf(kEndFileSignatureGuid);
    if (index == -1)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Signature not found in file [%1]", filename);
        return QByteArray();
    }

    return data.mid(index + sizeof(kEndFileSignatureGuid));
}
