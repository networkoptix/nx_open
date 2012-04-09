#include "sign_helper.h"
#include "utils/common/util.h"
#include "licensing/license.h"
#include "utils/common/yuvconvert.h"
#include "utils/common/scoped_painter_rollback.h"

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
    SQUARE_SIZE = roundDown(SQUARE_SIZE, 2);
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

void QnSignHelper::updateDigest(AVCodecContext* srcCodec, EVP_MD_CTX* mdctx, const quint8* data, int size)
{
    if (srcCodec == 0 || srcCodec->codec_id != CODEC_ID_H264) {
        EVP_DigestUpdate(mdctx, (const char*) data, size);
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
            curSize = qMin(curSize, dataEnd - curNal);
            EVP_DigestUpdate(mdctx, (const char*) curNal, curSize);
            curNal += curSize;
        }
    }
    else {
        // prefix is 00 00 01 code
        const quint8* curNal = NALUnit::findNextNAL(data, dataEnd);
        while(curNal < dataEnd)
        {
            const quint8* nextNal = NALUnit::findNALWithStartCode(curNal, dataEnd, true);
            EVP_DigestUpdate(mdctx, (const char*) curNal, nextNal - curNal);
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

void QnSignHelper::draw(QPainter& painter, const QSize& paintSize, bool drawText)
{
    QnScopedPainterTransformRollback rollback(&painter);

    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    painter.fillRect(0,0, paintSize.width(), paintSize.height()/2, Qt::white);
    painter.fillRect(0,paintSize.height()/2, paintSize.width(), paintSize.height()/2, Qt::blue);

    QString versionStr = qApp->applicationName().append(" v").append(qApp->applicationVersion());

    if (drawText)
    {
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
        painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*4), QString("Checksum: ").append(m_sign.toHex()));
    }

    if (m_logo.width() > 0) {
        QPixmap logo = m_logo.scaledToWidth(paintSize.width()/8, Qt::SmoothTransformation);
        painter.drawPixmap(QPoint(paintSize.width() - logo.width() - text_x_offs, paintSize.height()/2 - logo.height() - text_y_offs), logo);
    }

    // draw Hash
    int signBits = m_sign.length()*8;
    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;

    int SQUARE_SIZE = getSquareSize(paintSize.width(), paintSize.height(), signBits);


    int drawWidth = SQUARE_SIZE * colCnt;
    int drawheight = SQUARE_SIZE * rowCnt;
    painter.translate((paintSize.width() - drawWidth)/2, paintSize.height()/2 + (paintSize.height()/2-drawheight)/2);


    //painter.fillRect(0, 0, SQUARE_SIZE*colCnt, SQUARE_SIZE*rowCnt, Qt::white);
    painter.setOpacity(m_opacity);
    painter.fillRect(0, 0, SQUARE_SIZE*colCnt, SQUARE_SIZE*rowCnt, m_signBackground);

    //if (m_roundRectPixmap.width() != SQUARE_SIZE)
    {
        int cOffs = SQUARE_SIZE/16;
        m_roundRectPixmap = QPixmap(SQUARE_SIZE, SQUARE_SIZE);
        QPainter tmpPainter(&m_roundRectPixmap);
            tmpPainter.fillRect(0,0, SQUARE_SIZE, SQUARE_SIZE, Qt::blue);
            tmpPainter.setOpacity(m_opacity);
        tmpPainter.fillRect(0,0, SQUARE_SIZE, SQUARE_SIZE, m_signBackground);
        
        tmpPainter.setOpacity(1.0);
        tmpPainter.setBrush(QColor(0,0,0));
        tmpPainter.setPen(QColor(128,128,128));
        tmpPainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        tmpPainter.drawRoundedRect(QRect(cOffs, cOffs, SQUARE_SIZE-cOffs*2, SQUARE_SIZE-cOffs*2), SQUARE_SIZE/4, SQUARE_SIZE/4);
    }

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

    if (useSSE3() && frame->format == PIX_FMT_YUV420P)
    {
        bgra_to_yv12_sse2_intr(img.bits(), img.bytesPerLine(),
            frame->data[0], frame->data[1], frame->data[2],
            frame->linesize[0], frame->linesize[1], frame->width, frame->height, false);
    }
    else {
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

void QnSignHelper::fillH264EncoderParams(const QByteArray& srcCodecExtraData, AVCodecContext* avctx, AVDictionary* &options)
{
    SPSUnit sps;
    PPSUnit pps;
    bool spsReady;
    bool ppsReady;
    extractSpsPpsFromPrivData(srcCodecExtraData, sps, pps, spsReady, ppsReady);
    if (spsReady && ppsReady)
    {
        if (sps.profile_idc >= 100)
            av_dict_set(&options, "profile", "high", 0); 
        else if (sps.pic_order_cnt_type == 0)
            av_dict_set(&options, "profile", "main", 0); 
        else
            av_dict_set(&options, "profile", "baseline", 0); 

        int bframes = 1;
        int gopLen = 250;
        QString x264Params("level=%1:ref=%2:cabac=%3:keyint_min=%4:keyint=%4:subme=5:b-pyramid=none:bframes=%5:8x8dct=%6");
        x264Params = x264Params.arg(51).arg(sps.num_ref_frames).arg(pps.entropy_coding_mode_flag).arg(gopLen).arg(bframes).arg(pps.transform_8x8_mode_flag);
        av_dict_set(&options, "x264opts", x264Params.toAscii().data(), 0);
    }
}

int QnSignHelper::correctX264Bitstream(const QByteArray& srcCodecExtraData, AVCodecContext* videoCodecCtx, quint8* videoBuf, int out_size, int videoBufSize)
{
    SPSUnit oldSps, newSps;
    PPSUnit oldPps, newPps;
    bool spsReady;
    bool ppsReady;
    extractSpsPpsFromPrivData(srcCodecExtraData, oldSps, oldPps, spsReady, ppsReady);
    if (!spsReady || !ppsReady)
        return out_size;
    int oldLen = oldSps.log2_max_pic_order_cnt_lsb;
    extractSpsPpsFromPrivData(QByteArray((const char*)videoCodecCtx->extradata,videoCodecCtx->extradata_size), newSps, newPps, spsReady, ppsReady);
    if (!spsReady || !ppsReady)
        return out_size;
    int newLen= newSps.log2_max_pic_order_cnt_lsb;
    Q_ASSERT(newLen <= oldLen);

    const quint8* bufEnd = videoBuf + out_size;
    SliceUnit frame;
    quint8* nalData = (quint8*) NALUnit::findNextNAL(videoBuf, bufEnd);
    int nalCodeLen = nalData - videoBuf;
    frame.decodeBuffer(nalData, bufEnd);
    frame.m_shortDeserializeMode = false;
    frame.deserialize(&newSps, &newPps);
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

int QnSignHelper::correctNalPrefix(const QByteArray& srcCodecExtraData, quint8* videoBuf, int out_size, int videoBufSize)
{
    int x264PrefixLen = NALUnit::findNextNAL(videoBuf, videoBuf+out_size) - videoBuf;

    const quint8* spsPpsData = (quint8*) srcCodecExtraData.data();
    const quint8* spsPpsEnd = spsPpsData + srcCodecExtraData.size();
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

QnCompressedVideoDataPtr QnSignHelper::createSgnatureFrame(AVCodecContext* srcCodec)
{
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
    avpicture_alloc((AVPicture*) frame, videoCodecCtx->pix_fmt, roundUp((quint32)videoCodecCtx->width, 32), videoCodecCtx->height);

    AVCodec* videoCodec = avcodec_find_encoder(videoCodecCtx->codec_id);


    AVDictionary* options = 0;
    if (videoCodecCtx->codec_id == CODEC_ID_H264)
        fillH264EncoderParams(srcCodecExtraData, videoCodecCtx, options); // make X264 frame compatible with existing stream

    if (avcodec_open2(videoCodecCtx, videoCodec, &options) < 0)
    {
        qCritical() << "Can't initialize video encoder";
        return QnCompressedVideoDataPtr();
    }


    int videoBufSize = 1024*1024*4;
    quint8* videoBuf = new quint8[videoBufSize];

    drawOnSignFrame(frame);

    int out_size = avcodec_encode_video(videoCodecCtx, videoBuf, videoBufSize, frame);
    if (out_size == 0)
        out_size = avcodec_encode_video(videoCodecCtx, videoBuf, videoBufSize, 0); // flush encoder buffer

    quint8* dataStart = videoBuf;
    if (videoCodecCtx->codec_id == CODEC_ID_H264) 
    {
        // skip x264 SEI message
        dataStart = (quint8*) NALUnit::findNALWithStartCode(videoBuf+4, videoBuf + out_size, true); // remove H.264 SEI message
        out_size -= dataStart-videoBuf;

        // make X264 frame compatible with existing stream
        out_size = correctX264Bitstream(srcCodecExtraData, videoCodecCtx, dataStart, out_size, videoBufSize-(dataStart-videoBuf)); 

        // change nal prefix if need
        out_size = correctNalPrefix(srcCodecExtraData, dataStart, out_size, videoBufSize-(dataStart-videoBuf)); 
    }

    QnCompressedVideoDataPtr generatedFrame(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
    generatedFrame->compressionType = videoCodecCtx->codec_id;
    generatedFrame->data.write((const char*) dataStart, out_size);
    generatedFrame->flags = AV_PKT_FLAG_KEY;
    generatedFrame->channelNumber = 0; 

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
