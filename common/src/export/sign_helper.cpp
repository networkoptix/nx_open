#include "sign_helper.h"
#include "utils/common/util.h"
#include "licensing/license.h"
#include "utils/common/scoped_painter_rollback.h"
#include "utils/common/math.h"
#include "utils/common/base.h"

extern "C" {
#ifdef WIN32
#define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include "libavutil/pixdesc.h"
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif
};


static const int text_x_offs = 16;
static const int text_y_offs = 16;

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

QnSignHelper::QnSignHelper():
    m_cachedMetric(QFont())
{
    m_opacity = 1.0;
    m_signBackground = Qt::white;
}

void QnSignHelper::updateDigest(AVCodecContext* srcCodec, QnCryptographicHash &ctx, const quint8* data, int size)
{
    if (srcCodec == 0 || srcCodec->codec_id != CODEC_ID_H264) {
        ctx.addData((const char*) data, size);
        return;
    }

    // skip nal prefixes (sometimes it is 00 00 01 code, sometimes unitLen)
    const quint8* dataEnd = data + size;

    if (srcCodec->extradata_size >= 7 && srcCodec->extradata[0] == 1)
    {
        // prefix is unit len
        int reqUnitSize = (srcCodec->extradata[4] & 0x03) + 1;

        const quint8* curNal = data;
        while (curNal < dataEnd - reqUnitSize)
        {
            int curSize = 0;
            for (int i = 0; i < reqUnitSize; ++i) 
                curSize = (curSize << 8) + curNal[i];
            curNal += reqUnitSize;
            curSize = qMin(curSize, (int)(dataEnd - curNal));
            ctx.addData((const char*) curNal, curSize);

            curNal += curSize;
        }
    }
    else {
        // prefix is 00 00 01 code
        const quint8* curNal = NALUnit::findNextNAL(data, dataEnd);
        while(curNal < dataEnd)
        {
            const quint8* nextNal = NALUnit::findNALWithStartCode(curNal, dataEnd, true);
            ctx.addData((const char*) curNal, nextNal - curNal);

            curNal = NALUnit::findNextNAL(nextNal, dataEnd);
        }
    }
}

QByteArray QnSignHelper::getSign(const AVFrame* frame, int signLen)
{
    static const int COLOR_THRESHOLD = 32;

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
    BitStreamWriter writer;
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
            const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[frame->format];
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

QFontMetrics QnSignHelper::updateFontSize(QPainter& painter, const QSize& paintSize)
{
    QString versionStr = qApp->applicationName().append(" v").append(qApp->applicationVersion());

    if (m_lastPaintSize == paintSize)
    {
        painter.setFont(m_cachedFont);
        return m_cachedMetric;
    }

    QFont font;
    QFontMetrics metric(font);
    for (int i = 0; i < 100; ++i)
    {
        font.setPointSize(font.pointSize() + 1);
        metric = QFontMetrics(font);
        int width = metric.width(versionStr);
        int height = metric.height();
        if (width >= paintSize.width()/2 || height >= (paintSize.height()/2-text_y_offs) / 4)
            break;
    }
    painter.setFont(font);

    m_lastPaintSize = paintSize;
    m_cachedFont = font;
    m_cachedMetric = metric;
    return metric;
}

void QnSignHelper::drawTextLine(QPainter& painter, const QSize& paintSize,int lineNum, const QString& text)
{
    QFontMetrics metric = updateFontSize(painter, paintSize);
    painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*lineNum), text);
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
    QnScopedPainterTransformRollback rollback(&painter);

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
        QString versionStr = qApp->applicationName().append(" v").append(qApp->applicationVersion());
        QFontMetrics metric = updateFontSize(painter, paintSize);

        //painter.drawText(QPoint(text_x_offs, text_y_offs), qApp->organizationName());
        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()), versionStr);
        QString hid = qnLicensePool->getLicenses().hardwareId();
        if (hid.isEmpty())
            hid = "Unknown";
        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*2), QString("Hardware ID: ").append(hid));
        QList<QnLicensePtr> list = qnLicensePool->getLicenses().licenses();
        QString licenseName("FREE license");
        foreach (QnLicensePtr license, list)
        {
            if (license->name() != "FREE")
                licenseName = license->name();
        }

        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*3), QString("Licensed to: ").append(licenseName));
        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*4), QString("Watermark: ").append(m_sign.toHex()));
    }

    // draw Hash
    int signBits = m_sign.length()*8;
    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;

    int SQUARE_SIZE = getSquareSize(paintSize.width(), paintSize.height(), signBits);


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
}

void QnSignHelper::draw(QImage& img, bool drawText)
{
    QPainter painter(&img);
    draw(painter, img.size(), drawText);
    //img.save("c:/test.bmp");
}

void QnSignHelper::drawOnSignFrame(AVFrame* frame)
{
    quint8* imgBuffer = (quint8*) qMallocAligned(frame->linesize[0]*frame->height*4, 32);
    QImage img(imgBuffer, frame->width, frame->height, frame->linesize[0]*4, QImage::Format_ARGB32);
    draw(img, true);

    SwsContext* scaleContext = sws_getContext(frame->width, frame->height, PIX_FMT_BGRA, frame->width, frame->height, (PixelFormat) frame->format, SWS_POINT, NULL, NULL, NULL);
    if (scaleContext)
    {
        quint8* srcData[4];
        srcData[0] = img.bits();
        srcData[1] = srcData[2] = srcData[3] = 0;
        int srcLineSize[4];
        srcLineSize[0] = img.bytesPerLine();
        srcLineSize[1] = srcLineSize[2] = srcLineSize[3] = 0;
        sws_scale(scaleContext,
            srcData, srcLineSize, 
            0, frame->height, 
            frame->data, frame->linesize);

        sws_freeContext(scaleContext);
    }

    qFreeAligned(imgBuffer);

}

void QnSignHelper::extractSpsPpsFromPrivData(const QByteArray& data, SPSUnit& sps, PPSUnit& pps, bool& spsReady, bool& ppsReady)
{
    spsReady = false;
    ppsReady = false;

    const quint8* spsPpsData = (quint8*) data.data();
    const quint8* spsPpsEnd = spsPpsData + data.size();
    if (data.size() >= 7)
    {
        if (spsPpsData[0] == 1)
        {
            int cnt = spsPpsData[5] & 0x1f; // Number of sps
            spsPpsData += 6;
            for (int i = 0; i < cnt; ++i)
            {
                int spsSize = (spsPpsData[0]<<8) + spsPpsData[1];
                spsPpsData += 2;
                sps.decodeBuffer(spsPpsData, spsPpsData  + spsSize);
                sps.deserialize();
                spsReady = true;
                spsPpsData += spsSize;
            }

            cnt = *spsPpsData++; // Number of pps
            for (int i = 0; i < cnt; ++i)
            {
                int ppsSize = (spsPpsData[0]<<8) + spsPpsData[1];
                spsPpsData += 2;
                pps.decodeBuffer(spsPpsData, spsPpsData  + ppsSize);
                pps.deserialize();
                ppsReady = true;
                spsPpsData += ppsSize;
            }

        }
        else {
            const quint8* curData = NALUnit::findNextNAL(spsPpsData, spsPpsEnd);
            while (curData < spsPpsEnd) {
                const quint8* nalEnd = NALUnit::findNALWithStartCode(curData+4, spsPpsEnd, true);

                if ((*curData & 0x1f) == nuSPS) {
                    sps.decodeBuffer(curData, nalEnd);
                    sps.deserialize();
                    spsReady = true;
                }
                else if ((*curData & 0x1f) == nuPPS) {
                    pps.decodeBuffer(curData, nalEnd);
                    pps.deserialize();
                    ppsReady = true;
                }

                curData = NALUnit::findNextNAL(nalEnd, spsPpsEnd);
            }
        }
    }
}

QString QnSignHelper::fillH264EncoderParams(const QByteArray& srcCodecExtraData, AVCodecContext* /*avctx*/)
{
    QString result;
    SPSUnit sps;
    PPSUnit pps;
    bool spsReady;
    bool ppsReady;
    QString profile;
    extractSpsPpsFromPrivData(srcCodecExtraData, sps, pps, spsReady, ppsReady);
    if (spsReady && ppsReady)
    {
        if (sps.profile_idc >= 100)
            profile = "high";
        else if (sps.pic_order_cnt_type == 0)
            profile = "main";
        else
            profile = "baseline";

        int bframes = 1;
        if (sps.pic_order_cnt_type == 2)
            bframes = 0;
        int gopLen = 16;
        QString deblockFilter("");
        if (pps.deblocking_filter_control_present_flag == 0)
            deblockFilter = "--no-deblock";
        QString x264Params("--bitrate 20000 --profile %1 --level %2 --ref %3 --%4 --keyint %5 --subme 5 --b-pyramid none --bframes %6 --%7 --weightp %8 %9");
        result = x264Params.arg(profile).arg(51).arg(sps.num_ref_frames).arg(pps.entropy_coding_mode_flag ? "cabac" : "no-cabac").
                                arg(gopLen).arg(bframes).arg(pps.transform_8x8_mode_flag ? "8x8dct" : "no-8x8dct").arg(pps.weighted_pred_flag).arg(deblockFilter);
    }
    return result;
}

int QnSignHelper::correctX264Bitstream(const QByteArray& srcCodecExtraData, AVCodecContext* /*videoCodecCtx*/, quint8* videoBuf, int out_size, int videoBufSize)
{
    SPSUnit oldSps, newSps;
    PPSUnit oldPps, newPps;
    bool spsReady;
    bool ppsReady;
    extractSpsPpsFromPrivData(srcCodecExtraData, oldSps, oldPps, spsReady, ppsReady);
    if (!spsReady || !ppsReady)
        return out_size;
    int oldLen = oldSps.log2_max_pic_order_cnt_lsb;
    extractSpsPpsFromPrivData(QByteArray((const char*)videoBuf,out_size), newSps, newPps, spsReady, ppsReady);
    if (!spsReady || !ppsReady)
        return -1;

    quint8* bufEnd = videoBuf + out_size;
    quint8* curNal = (quint8*) NALUnit::findNextNAL(videoBuf, bufEnd);
    while (curNal < bufEnd)
    {
        quint8 nalType = *curNal & 0x1f;
        if (nalType == nuSliceIDR)
        {
            curNal = (quint8*) NALUnit::findNALWithStartCode(curNal-4, bufEnd, true);
            memmove(videoBuf, curNal, bufEnd - curNal);
            out_size -= curNal- videoBuf;
            break;
        }
        curNal = (quint8*) NALUnit::findNextNAL(curNal, bufEnd);
    }

    int newLen= newSps.log2_max_pic_order_cnt_lsb;
    Q_ASSERT(newLen <= oldLen);

    bufEnd = videoBuf + out_size;
    SliceUnit frame;
    quint8* nalData = (quint8*) NALUnit::findNextNAL(videoBuf, bufEnd);
    int nalCodeLen = nalData - videoBuf;
    frame.decodeBuffer(nalData, bufEnd);
    frame.m_shortDeserializeMode = false;
    if (frame.deserialize(&newSps, &newPps) != 0)
        return -1;
    if (!frame.moveHeaderField(frame.m_picOrderBitPos+8, oldLen, newLen)) 
        return out_size;

    oldLen = oldSps.log2_max_frame_num;
    newLen = newSps.log2_max_frame_num;
    Q_ASSERT(newLen <= oldLen);
    if (!frame.moveHeaderField(frame.m_frameNumBitPos+8, oldLen, newLen)) 
        return out_size;

    out_size = frame.encodeNAL(nalData, videoBufSize-nalCodeLen) + nalCodeLen;

    SliceUnit sliceTmp;
    sliceTmp.decodeBuffer(nalData, bufEnd);
    sliceTmp.m_shortDeserializeMode = false;
    sliceTmp.deserialize(&oldSps, &oldPps);

    return out_size;
}

int QnSignHelper::correctNalPrefix(const QByteArray& srcCodecExtraData, quint8* videoBuf, int out_size, int /*videoBufSize*/)
{
    int x264PrefixLen = NALUnit::findNextNAL(videoBuf, videoBuf+out_size) - videoBuf;

    const quint8* spsPpsData = (quint8*) srcCodecExtraData.data();
    if (srcCodecExtraData.size() < 7)
        return out_size;

    if (spsPpsData[0] == 0)
    {

    }
    else {
        int reqUnitSize = (spsPpsData[4] & 0x03) + 1;
        memmove(videoBuf+x264PrefixLen, videoBuf+reqUnitSize, out_size - x264PrefixLen);
        out_size += reqUnitSize - x264PrefixLen;
        int tmp = out_size - reqUnitSize;
        for (int i = 0; i < reqUnitSize; ++i) 
        {
            videoBuf[reqUnitSize-1-i] = (quint8) tmp;
            tmp >>= 8;
        }
    }
    return out_size;
}

int QnSignHelper::runX264Process(AVFrame* frame, QString optionStr, quint8* rezBuffer)
{
    QTemporaryFile file;
    if (!file.open())
        return -1;
    QString tempName = file.fileName();

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[frame->format];
    file.write((const char*) frame->data[0], frame->linesize[0]*frame->height);
    file.write((const char*) frame->data[1], frame->linesize[1]*frame->height/(1 << descr->log2_chroma_h));
    file.write((const char*) frame->data[2], frame->linesize[2]*frame->height/(1 << descr->log2_chroma_h));
    file.close();

    QString executableName = closeDirPath(QFileInfo(qApp->argv()[0]).absolutePath()) + QString("x264");
    QString command("\"%1\" %2 -o \"%3\" --input-res %4x%5 \"%6\"");
    QString outFileName(tempName+QString(".264"));
    command = command.arg(executableName).arg(optionStr).arg(outFileName).arg(frame->width).arg(frame->height).arg(tempName);
    int execResult = QProcess::execute(command);
    if (execResult != 0)
        return -1;

    QFile rezFile(outFileName);
    if (!rezFile.open(QIODevice::ReadOnly))
        return -1;
    QByteArray rezData = rezFile.readAll();
    rezFile.close();
    QFile::remove(tempName);
    QFile::remove(outFileName);
    memcpy(rezBuffer, rezData.data(), rezData.size());
    return rezData.size();
}

int QnSignHelper::removeH264SeiMessage(quint8* buffer, int size)
{
    if (size < 4)
        return size;
    quint8* bufEnd = buffer + size;
    quint8* curPtr = (quint8*) NALUnit::findNextNAL(buffer, bufEnd); // remove H.264 SEI message
    while (curPtr < bufEnd)
    {
        quint8 nal = *curPtr & 0x1f;
        if (nal == nuSEI)
        {
            const quint8* nextNal = NALUnit::findNALWithStartCode(curPtr, bufEnd, true);
            curPtr = (quint8*) NALUnit::findNALWithStartCode(curPtr-4, bufEnd, true);
            memmove(curPtr, nextNal, bufEnd - nextNal);
            size -= nextNal - curPtr;
            return size;
        }
        curPtr = (quint8*) NALUnit::findNextNAL(curPtr, bufEnd); // remove H.264 SEI message
    }

    return size;
}

QnCompressedVideoDataPtr QnSignHelper::createSgnatureFrame(AVCodecContext* srcCodec)
{
    QnCompressedVideoDataPtr generatedFrame;
    QByteArray srcCodecExtraData((const char*) srcCodec->extradata, srcCodec->extradata_size);

    AVCodecContext* videoCodecCtx = avcodec_alloc_context(); //m_formatCtx->streams[0]->codec;
    videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    videoCodecCtx->codec_id = srcCodec->codec_id;
    videoCodecCtx->width = srcCodec->width;
    videoCodecCtx->height = srcCodec->height;
    videoCodecCtx->pix_fmt = srcCodec->pix_fmt;
    videoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    videoCodecCtx->bit_rate = 1024*1024*10;
    videoCodecCtx->gop_size = 32;
    videoCodecCtx->time_base.num = 1;
    videoCodecCtx->time_base.den = 30;


    AVFrame* frame = avcodec_alloc_frame();
    frame->width = videoCodecCtx->width;
    frame->height = videoCodecCtx->height;
    frame->format = videoCodecCtx->pix_fmt;
    avpicture_alloc((AVPicture*) frame, videoCodecCtx->pix_fmt, qPower2Ceil((quint32)videoCodecCtx->width, 32), videoCodecCtx->height);

    AVCodec* videoCodec = avcodec_find_encoder(videoCodecCtx->codec_id);


    //AVDictionary* options = 0;

    int videoBufSize = 1024*1024*4;
    quint8* videoBuf = new quint8[videoBufSize];

    drawOnSignFrame(frame);

    int out_size = 0;
    if (videoCodecCtx->codec_id == CODEC_ID_H264)
    {
        // To avoid X.264 GPL restriction run x264 as separate process
        QString optionStr = fillH264EncoderParams(srcCodecExtraData, videoCodecCtx); // make X264 frame compatible with existing stream
        out_size = runX264Process(frame, optionStr, videoBuf);
        if (out_size == -1)
            goto error_label;
    }
    else {
        if (avcodec_open2(videoCodecCtx, videoCodec, 0) < 0)
        {
            qWarning() << "Can't initialize video encoder";
            goto error_label;
        }

        out_size = avcodec_encode_video(videoCodecCtx, videoBuf, videoBufSize, frame);
        if (out_size == 0)
            out_size = avcodec_encode_video(videoCodecCtx, videoBuf, videoBufSize, 0); // flush encoder buffer
    }

    if (videoCodecCtx->codec_id == CODEC_ID_H264) 
    {
        // skip x264 SEI message
        out_size = removeH264SeiMessage(videoBuf, out_size);

        // make X264 frame compatible with existing stream
        out_size = correctX264Bitstream(srcCodecExtraData, videoCodecCtx, videoBuf, out_size, videoBufSize); 
        if (out_size == -1)
            goto error_label;
            // change nal prefix if need
        out_size = correctNalPrefix(srcCodecExtraData, videoBuf, out_size, videoBufSize); 
    }

    generatedFrame = QnCompressedVideoDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
    generatedFrame->compressionType = videoCodecCtx->codec_id;
    generatedFrame->data.write((const char*) videoBuf, out_size);
    generatedFrame->flags = AV_PKT_FLAG_KEY;
    generatedFrame->channelNumber = 0; 
error_label:
    delete [] videoBuf;
    avcodec_close(videoCodecCtx);
    av_free(frame);
    av_free(videoCodecCtx);

    return generatedFrame;
}

void QnSignHelper::setLogo(QPixmap logo)
{
    m_logo = logo;
}

void QnSignHelper::setSign(const QByteArray& sign)
{
    m_sign = sign;
}
