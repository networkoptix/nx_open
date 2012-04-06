#include "stream_recorder.h"
#include "core/resource/resource_consumer.h"
#include "core/datapacket/datapacket.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "utils/common/util.h"
#include "decoders/video/ffmpeg.h"
#include "utils/media/nalUnits.h"
#include "licensing/license.h"
#include "utils/common/yuvconvert.h"

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4352;

static const int STORE_QUEUE_SIZE = 50;

static const char hashMethod[] = "md5";
static const char HASH_MAGIC[] = "RhjrjLbkMxTujHI!";

extern QMutex global_ffmpeg_mutex;

QnStreamRecorder::QnStreamRecorder(QnResourcePtr dev):
QnAbstractDataConsumer(STORE_QUEUE_SIZE),
m_device(dev),
m_firstTime(true),
m_packetWrited(false),
m_formatCtx(0),
m_truncateInterval(0),
m_startDateTime(AV_NOPTS_VALUE),
m_endDateTime(AV_NOPTS_VALUE),
m_startOffset(0),
m_forceDefaultCtx(true),
m_prebufferingUsec(0),
m_stopOnWriteError(true),
m_waitEOF(false),
m_endOfData(false),
m_lastProgress(-1),
m_EofDateTime(AV_NOPTS_VALUE),
m_needCalcSignature(false),
m_mdctx(0)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
}

QnStreamRecorder::~QnStreamRecorder()
{
    stop();
	close();
    delete m_mdctx;
}

void QnStreamRecorder::close()
{
    if (m_formatCtx) 
    {
        if (m_startDateTime != AV_NOPTS_VALUE)
        {
            qint64 fileLength = m_startDateTime != AV_NOPTS_VALUE  ? (m_endDateTime - m_startDateTime)/1000 : 0;
            fileFinished(fileLength, m_fileName, m_mediaProvider);
        }

        if (m_packetWrited)
            av_write_trailer(m_formatCtx);

        QMutexLocker mutex(&global_ffmpeg_mutex);
        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
        {
            if (m_formatCtx->streams[i]->codec->codec)
                avcodec_close(m_formatCtx->streams[i]->codec);
        }
        if (m_formatCtx->pb)
            avio_close(m_formatCtx->pb);
        avformat_free_context(m_formatCtx);
        m_formatCtx = 0;

    }
    m_packetWrited = false;
    m_endDateTime = m_startDateTime = AV_NOPTS_VALUE;

    markNeedKeyData();
	m_firstTime = true;
}

void QnStreamRecorder::markNeedKeyData()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_gotKeyFrame[i] = false;
}

void QnStreamRecorder::flushPrebuffer()
{
    while (!m_prebuffer.isEmpty())
    {
        QnAbstractMediaDataPtr d = m_prebuffer.dequeue();
        if (needSaveData(d))
            saveData(d);
        else
            markNeedKeyData();
    }
}

bool QnStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr md = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!md)
        return true; // skip unknown data

    if (m_EofDateTime != AV_NOPTS_VALUE && md->timestamp > m_EofDateTime)
    {
        if (!m_endOfData) 
        {
            bool isOK = true;
            if (m_needCalcSignature) 
                isOK = addSignatureFrame(m_lastErrMessage);

            if (isOK)
                emit recordingFinished(m_fileName);
            else
                emit recordingFailed(m_lastErrMessage);
            m_endOfData = true;
        }

        stop();
        return true;
    }

    m_prebuffer << md;
    while (!m_prebuffer.isEmpty() && md->timestamp-m_prebuffer.first()->timestamp >= m_prebufferingUsec)
    {
        QnAbstractMediaDataPtr d = m_prebuffer.dequeue();
        if (needSaveData(d))
            saveData(d);
        else
            markNeedKeyData();
    }

    if (m_waitEOF && m_dataQueue.size() == 0) {
        close();
        m_waitEOF = false;
    }

    if (m_EofDateTime != AV_NOPTS_VALUE && m_EofDateTime > m_startDateTime)
    {
        int progress = ((md->timestamp - m_startDateTime)*100ll) /(m_EofDateTime - m_startDateTime);
        if (progress != m_lastProgress) {
            emit recordingProgress(progress);
            m_lastProgress = progress;
        }
    }

    return true;
}

bool QnStreamRecorder::saveData(QnAbstractMediaDataPtr md)
{
    if (m_endDateTime != AV_NOPTS_VALUE && md->timestamp - m_endDateTime > MAX_FRAME_DURATION*2*1000ll && m_truncateInterval > 0) {
        // if multifile recording allowed, recreate file if recording hole is detected
        qDebug() << "Data hole detected for camera" << m_device->getUniqueId() << ". Diff between packets=" << (md->timestamp - m_endDateTime)/1000 << "ms";
        close();
    }
    else if (m_startDateTime != AV_NOPTS_VALUE && md->timestamp - m_startDateTime > m_truncateInterval*3 && m_truncateInterval > 0) {
        // if multifile recording allowed, recreate file if recording hole is detected
        qDebug() << "Too long time when no I-frame detected (file length exceed " << (md->timestamp - m_startDateTime)/1000000 << "sec. Close file";
        close();
    }

    if (md->dataType == QnAbstractMediaData::META_V1)
        return saveMotion(md);

    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(md);
    if (!vd)
        return true; // ignore audio data

    if (vd && !m_gotKeyFrame[vd->channelNumber] && !(vd->flags & AV_PKT_FLAG_KEY))
        return true; // skip data

    if (m_firstTime)
    {
        if (vd == 0)
            return true; // skip audio packets before first video packet
        if (!initFfmpegContainer(vd))
        {
            emit recordingFailed(m_lastErrMessage);
            if (m_stopOnWriteError)
            {
                m_needStop = true;
                return false;
            }
            else {
                return true; // ignore data
            }
        }

        m_firstTime = false;
        emit recordingStarted();
    }

    int channel = md->channelNumber;
    if (channel >= m_formatCtx->nb_streams)
        return true; // skip packet

    if (md->flags & AV_PKT_FLAG_KEY)
    {
        m_gotKeyFrame[channel] = true;
        if (m_truncateInterval > 0 && md->timestamp - m_startDateTime > m_truncateInterval)
        {
            m_endDateTime = md->timestamp;
            close();
            m_endDateTime = m_startDateTime = md->timestamp;

            return saveData(md);
        }
    }

    AVPacket avPkt;
    av_init_packet(&avPkt);
    AVStream* stream = m_formatCtx->streams[channel];
    AVRational srcRate = {1, 1000000};
    Q_ASSERT(stream->time_base.num && stream->time_base.den);

    m_endDateTime = md->timestamp;

    avPkt.pts = av_rescale_q(md->timestamp-m_startDateTime, srcRate, stream->time_base);
    avPkt.dts = avPkt.pts;
    if(md->flags & AV_PKT_FLAG_KEY)
        avPkt.flags |= AV_PKT_FLAG_KEY;
    avPkt.stream_index= channel;
    avPkt.data = (quint8*) md->data.data();
    avPkt.size = md->data.size();

    if (av_write_frame(m_formatCtx, &avPkt) < 0) 
        cl_log.log(QLatin1String("AV packet write error"), cl_logWARNING);
    else {
        m_packetWrited = true;
        if (m_needCalcSignature)
            EVP_DigestUpdate(m_mdctx, (const char*)avPkt.data, avPkt.size);
    }

    return true;
};

void QnStreamRecorder::endOfRun()
{
	close();
}

bool QnStreamRecorder::initFfmpegContainer(QnCompressedVideoDataPtr mediaData)
{
    m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (mediaData->dataProvider);
    Q_ASSERT(m_mediaProvider);

    m_endDateTime = m_startDateTime = mediaData->timestamp;

    //QnResourcePtr resource = mediaData->dataProvider->getResource();
    // allocate container
    AVOutputFormat * outputCtx = av_guess_format("matroska",NULL,NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = "No MKV container in FFMPEG library";
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QString fileExt = QString(outputCtx->extensions).split(',')[0];
    m_fileName = fillFileName(m_mediaProvider);
    if (m_fileName.isEmpty()) 
        return false;

    m_fileName += QString(".") + fileExt;
    QString url = QString("ufile:") + m_fileName;

    global_ffmpeg_mutex.lock();
    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, url.toUtf8().constData());
    global_ffmpeg_mutex.unlock();

    if (err < 0) {
        m_lastErrMessage = QString("Can't create output file '") + m_fileName + QString("' for video recording.");
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QnMediaResourcePtr mediaDev = qSharedPointerDynamicCast<QnMediaResource>(m_device);
    const QnVideoResourceLayout* layout = mediaDev->getVideoLayout(m_mediaProvider);
    QString layoutStr = QnArchiveStreamReader::serializeLayout(layout);
    const QnResourceAudioLayout* audioLayout = mediaDev->getAudioLayout(m_mediaProvider);

    {
        QMutexLocker mutex(&global_ffmpeg_mutex);
        
        if (av_set_parameters(m_formatCtx, NULL) < 0) {
            m_lastErrMessage = "Can't initialize output format parameters for recording video camera";
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }
        av_metadata_set2(&m_formatCtx->metadata, "video_layout", layoutStr.toAscii().data(), 0);
        av_metadata_set2(&m_formatCtx->metadata, "start_time", QString::number(m_startOffset+mediaData->timestamp/1000).toAscii().data(), 0);

        m_formatCtx->start_time = mediaData->timestamp;

        int vChannels = layout->numberOfChannels();

        for (unsigned int i = 0; i < vChannels; ++i) 
        {
            AVStream* videoStream = av_new_stream(m_formatCtx, DEFAULT_VIDEO_STREAM_ID+i);
            if (videoStream == 0)
            {
                m_lastErrMessage = "Can't allocate output stream for recording.";
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            AVCodecContext* srcContext = mediaData->context->ctx();
            AVCodecContext* videoCodecCtx = videoStream->codec;


            // m_forceDefaultCtx: for server archive, if file is recreated - we need to use default context.
            // for exporting AVI files we must use original context, so need to reset "force" for exporting purpose
            /*
            if (mediaData->context && !m_forceDefaultCtx && srcContext->width > 0)
            {
                avcodec_copy_context(videoCodecCtx, srcContext);
                videoStream->stream_copy = 1;
                m_srcCodecExtraData = QByteArray((const char*)mediaData->context->ctx()->extradata, mediaData->context->ctx()->extradata_size);
            }
            else
                */
                if (m_role == Role_FileExport)
            {
                // determine real width and height
                CLVideoDecoderOutput outFrame;
                CLFFmpegVideoDecoder decoder(mediaData->compressionType, mediaData);
                decoder.decode(mediaData, &outFrame);
                avcodec_copy_context(videoStream->codec, decoder.getContext());
                videoStream->stream_copy = 1;
                m_srcCodecExtraData = QByteArray((const char*)decoder.getContext()->extradata, decoder.getContext()->extradata_size);
            }
            else {
                videoCodecCtx->codec_id = mediaData->compressionType;
                videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
                videoCodecCtx->width = qMax(8,srcContext->width);
                videoCodecCtx->height = qMax(8,srcContext->height);
                
                if (mediaData->compressionType == CODEC_ID_MJPEG)
                    videoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
                else
                    videoCodecCtx->pix_fmt = PIX_FMT_YUV420P;
            }
            videoCodecCtx->bit_rate = 1000000 * 6;
            videoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
            AVRational defaultFrameRate = {1, 30};
            videoCodecCtx->time_base = defaultFrameRate;

            videoCodecCtx->sample_aspect_ratio.num = 1;
            videoCodecCtx->sample_aspect_ratio.den = 1;

            videoStream->sample_aspect_ratio = videoCodecCtx->sample_aspect_ratio;
            videoStream->first_dts = 0;

            //AVRational srcRate = {1, 1000000};
            //videoStream->first_dts = av_rescale_q(mediaData->timestamp, srcRate, stream->time_base);

            QString layoutData = QString::number(layout->v_position(i)) + QString(",") + QString(layout->h_position(i));
            av_metadata_set2(&videoStream->metadata, "layout_channel", QString::number(i).toAscii().data(), 0);
        }

        /*
        for (unsigned int i = 0; i < audioLayout->numberOfChannels(); ++i) 
        {
            AVStream* audioStream = av_new_stream(m_formatCtx, DEFAULT_AUDIO_STREAM_ID+i);
            if (audioStream == 0)
            {
                m_lastErrMessage = "Can't allocate output audio stream";
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            AVCodecContext* audioCodecCtx = audioStream->codec;
            audioCodecCtx->codec_id = audioLayout->getAudioTrackInfo(i).codec;
            audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
            audioCodecCtx->sample_rate = 48000;
            audioCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

            //AVRational srcRate = {1, 1000000};
            //audioStream->first_dts = av_rescale_q(mediaData->timestamp, srcRate, audioStream->time_base);
        }
        */

        if ((avio_open(&m_formatCtx->pb, url.toUtf8().constData(), AVIO_FLAG_WRITE)) < 0) 
        {
            m_lastErrMessage = "Can't allocate output stream for video recording.";
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }

        av_write_header(m_formatCtx);
    }
    fileStarted(m_startDateTime/1000, m_fileName, m_mediaProvider);

    return true;
}

void QnStreamRecorder::setTruncateInterval(int seconds)
{
    m_truncateInterval = seconds * 1000000ll;
}

void QnStreamRecorder::setFileName(const QString& fileName)
{
    m_fixedFileName = fileName;
}

void QnStreamRecorder::setStartOffset(qint64 value)
{
    m_startOffset = value;
}

void QnStreamRecorder::setPrebufferingUsec(int value)
{
    m_prebufferingUsec = value;
}

int QnStreamRecorder::getPrebufferingUsec() const
{
    return m_prebufferingUsec;
}


bool QnStreamRecorder::needSaveData(QnAbstractMediaDataPtr media)
{
    return true;
}

bool QnStreamRecorder::saveMotion(QnAbstractMediaDataPtr media)
{
    return true;
}

QString QnStreamRecorder::fillFileName(QnAbstractMediaStreamDataProvider* provider)
{
    Q_UNUSED(provider);
    return m_fixedFileName;
}

QString QnStreamRecorder::fixedFileName() const
{
    return m_fixedFileName;
}

void QnStreamRecorder::setEofDateTime(qint64 value)
{
    m_endOfData = false;
    m_EofDateTime = value;
    m_lastProgress = -1;
}

void QnStreamRecorder::setNeedCalcSignature(bool value)
{
    if (m_needCalcSignature == value)
        return;
    if (value)
    {
        const EVP_MD *md = EVP_get_digestbyname(hashMethod);
        if (md)
        {
            m_needCalcSignature = value;
            m_mdctx = new EVP_MD_CTX;
            EVP_MD_CTX_init(m_mdctx);
            EVP_DigestInit_ex(m_mdctx, md, NULL);
        }
        else {
            qCritical() << "Can't sign file because of '" << hashMethod << "' not found";
        }
    }
    else {
        delete m_mdctx;
        m_mdctx = 0;
    }
}

QByteArray QnStreamRecorder::getSignature() const
{
    quint8 md_value[EVP_MAX_MD_SIZE];
    quint32 md_len = 0;
    EVP_DigestFinal_ex(m_mdctx, md_value, &md_len);
    return QByteArray((const char*)md_value, md_len);
}

void QnStreamRecorder::drawOnSignFrame(AVFrame* frame)
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

    QByteArray sign = getSignature();

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

    int SQUARE_SIZE = qMin(frame->height/2/rowCnt, frame->width/colCnt);
    SQUARE_SIZE = roundDown(SQUARE_SIZE, 8) - 8;

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

void extractSpsPpsFromPrivData(const QByteArray& data, SPSUnit& sps, PPSUnit& pps, bool& spsReady, bool& ppsReady)
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

void QnStreamRecorder::fillH264EncoderParams(AVCodecContext* avctx, AVDictionary* &options)
{
    SPSUnit sps;
    PPSUnit pps;
    bool spsReady;
    bool ppsReady;
    extractSpsPpsFromPrivData(m_srcCodecExtraData, sps, pps, spsReady, ppsReady);
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

int QnStreamRecorder::correctX264Bitstream(AVCodecContext* videoCodecCtx, quint8* videoBuf, int out_size, int videoBufSize)
{
    SPSUnit oldSps, newSps;
    PPSUnit oldPps, newPps;
    bool spsReady;
    bool ppsReady;
    extractSpsPpsFromPrivData(m_srcCodecExtraData, oldSps, oldPps, spsReady, ppsReady);
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

int QnStreamRecorder::correctNalPrefix(quint8* videoBuf, int out_size, int videoBufSize)
{
    int x264PrefixLen = NALUnit::findNextNAL(videoBuf, videoBuf+out_size) - videoBuf;

    const quint8* spsPpsData = (quint8*) m_srcCodecExtraData.data();
    const quint8* spsPpsEnd = spsPpsData + m_srcCodecExtraData.size();
    if (m_srcCodecExtraData.size() < 7)
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

bool QnStreamRecorder::addSignatureFrame(QString& errorString)
{
    EVP_DigestUpdate(m_mdctx, HASH_MAGIC, sizeof(HASH_MAGIC));

    AVCodecContext* srcCodec = m_formatCtx->streams[0]->codec;
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
        fillH264EncoderParams(videoCodecCtx, options); // make X264 frame compatible with existing stream

    if (avcodec_open2(videoCodecCtx, videoCodec, &options) < 0)
    {
        errorString = QLatin1String("Can't initialize video encoder");
        return false;
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
        out_size = correctX264Bitstream(videoCodecCtx, dataStart, out_size, videoBufSize-(dataStart-videoBuf)); 

        // change nal prefix if need
        out_size = correctNalPrefix(dataStart, out_size, videoBufSize-(dataStart-videoBuf)); 
    }

    QnCompressedVideoDataPtr generatedFrame(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
    //generatedFrame->data.write((const char*)videoCodecCtx->extradata, videoCodecCtx->extradata_size);
    generatedFrame->data.write((const char*) dataStart, out_size);
    generatedFrame->timestamp = m_endDateTime + 100 * 1000ll;
    generatedFrame->flags = AV_PKT_FLAG_KEY;
    generatedFrame->channelNumber = 0; //layout->numberOfChannels();


    m_needCalcSignature = false; // prevent recursive calls
    for (int i = 0; i < 100; ++i) // 2 - work, 1 - work at VLC
    {
        saveData(generatedFrame);
        generatedFrame->timestamp += 100 * 1000ll;
    }
    m_needCalcSignature = true;

    delete [] videoBuf;
    avcodec_close(videoCodecCtx);
    av_free(frame);
    av_free(videoCodecCtx);

    return true;
}

void QnStreamRecorder::setRole(Role role)
{
    m_role = role;
    m_forceDefaultCtx = m_role == Role_ServerRecording;
}

void QnStreamRecorder::setSignLogo(QPixmap logo)
{
    m_logo = logo;
}
