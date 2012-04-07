#include "sign_frame.h"
#include "utils/common/util.h"
#include "licensing/license.h"
#include "utils/common/yuvconvert.h"

int getSquareSize(AVFrame* frame, int signBits)
{
    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;
    int SQUARE_SIZE = qMin((frame->height/2-16)/rowCnt, (frame->width-16)/colCnt);
    SQUARE_SIZE = roundDown(SQUARE_SIZE, 4);
    return SQUARE_SIZE;
}

float getAvgY(AVFrame* frame, const QRect& rect)
{
    quint8* curPtr = frame->data[0] + rect.top()*frame->linesize[0] + rect.left();
    float sum = 0;
    for (int y = 0; y < rect.height(); ++y)
    {
        for (int x = 0; x < rect.width(); ++x)
        {
            sum += curPtr[x];
        }
        curPtr += frame->linesize[0];
    }
    return sum / (rect.width() * rect.height());
}

QByteArray QnSignHelper::getSign(AVFrame* frame, int signLen)
{
    static const int COLOR_THRESHOLD = 32;

    int signBits = signLen*8;
    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;
    int SQUARE_SIZE = getSquareSize(frame, signBits);
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
            float avgColor = getAvgY(frame, checkRect);
            if (avgColor <= COLOR_THRESHOLD)
                writer.putBit(1);
            else if (avgColor >= (255-COLOR_THRESHOLD))
                writer.putBit(0);
            else 
                return QByteArray(); // invalid data
        }
    }
    return QByteArray((const char*)signArray, signLen/8);
}

void QnSignHelper::drawOnSignFrame(AVFrame* frame, const QByteArray& sign)
{
    quint8* imgBuffer = (quint8*) qMallocAligned(frame->linesize[0]*frame->height*4, 32);
    QImage img(imgBuffer, frame->width, frame->height, frame->linesize[0]*4, QImage::Format_ARGB32);
    QPainter painter(&img);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    painter.fillRect(0,0, img.width(), img.height()/2, QColor(255,255,255));
    painter.fillRect(0,img.height()/2, img.width(), img.height()/2, QColor(0,0,255));

    QString hIDStr = QString("Hardware ID: ").append(qnLicensePool->getLicenses().hardwareId());

    QFont font;
    QFontMetrics metric(font);
    for (int i = 0; i < 100; ++i)
    {
        font.setPointSize(font.pointSize() + 1);
        metric = QFontMetrics(font);
        int width = metric.width(hIDStr);
        if (width >= img.width()/2)
            break;
    }
    painter.setFont(font);

    int text_x_offs = 16;
    int text_y_offs = 16;
    //painter.drawText(QPoint(text_x_offs, text_y_offs), qApp->organizationName());
    painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()), qApp->applicationName().append(" v").append(qApp->applicationVersion()));
    painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*2), hIDStr);
    QList<QnLicensePtr> list = qnLicensePool->getLicenses().licenses();
    QString licenseName("FREE license");
    foreach (QnLicensePtr license, list)
    {
        if (license->name() != "FREE")
            licenseName = license->name();
    }

    painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*3), QString("Licensed to: ").append(licenseName));
    painter.drawText(QPoint(text_x_offs, text_y_offs + metric.height()*4), QString("Checksum: ").append(sign.toHex()));


    if (m_logo.width() > 0) {
        QPixmap logo = m_logo.scaledToWidth(img.width()/8, Qt::SmoothTransformation);
        painter.drawPixmap(QPoint(img.width() - logo.width() - text_x_offs, img.height()/2 - logo.height() - text_y_offs), logo);
    }

    // draw Hash
    int signBits = sign.length()*8;
    int rowCnt = signBits / 16;
    int colCnt = signBits / rowCnt;

    int SQUARE_SIZE = getSquareSize(frame, signBits);

    int drawWidth = SQUARE_SIZE * colCnt;
    int drawheight = SQUARE_SIZE * rowCnt;
    painter.translate((frame->width - drawWidth)/2, frame->height/2 + (frame->height/2-drawheight)/2);
    painter.setBrush(QColor(0,0,0));
    painter.setPen(QColor(128,128,128));
    for (int y = 0; y < rowCnt; ++y)
    {
        for (int x = 0; x < colCnt; ++x)
        {
            int bitNum = y*colCnt + x;
            unsigned char b = *((unsigned char*)sign.data() + bitNum/8 );
            bool isBit = b & (128 >> (bitNum&7));
            int cOffs = SQUARE_SIZE/16;
            painter.fillRect(x*SQUARE_SIZE, y*SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE, QColor(255,255,255));
            if (isBit)
                painter.drawRoundedRect(QRect(x*SQUARE_SIZE+cOffs, y*SQUARE_SIZE+cOffs, SQUARE_SIZE-cOffs*2, SQUARE_SIZE-cOffs*2), SQUARE_SIZE/4, SQUARE_SIZE/4);
        }
    }
    painter.end();
    //img.save("c:/test.bmp");


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

QnCompressedVideoDataPtr QnSignHelper::createSgnatureFrame(AVCodecContext* srcCodec, const QByteArray& hash)
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

    drawOnSignFrame(frame, hash);

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
