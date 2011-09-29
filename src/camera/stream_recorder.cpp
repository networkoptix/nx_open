#include "stream_recorder.h"

#include "device/device.h"
#include "base/bytearray.h"
#include "data/mediadata.h"
#include "streamreader/streamreader.h"
#include "util.h"
#include "base/nalUnits.h"
#include "device_plugins/archive/avi_files/avi_strem_reader.h"

#define FLUSH_SIZE (512*1024)
static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const char FULL_NAL_PREFIX[] = {0x00, 0x00, 0x00, 0x01};

CLStreamRecorder::CLStreamRecorder(CLDevice* dev):
CLAbstractDataProcessor(10),
m_device(dev),
m_firstTime(true),
m_tmpPacketBuffer(CL_MEDIA_ALIGNMENT, 0),
m_videoPacketWrited(false),
m_firstTimestamp(-1),
m_formatCtx(0),
m_outputCtx(0),
m_outputFile(0),
m_iocontext(0)
{
	m_version = 2;

	memset(m_data, 0, sizeof(m_data));
	memset(m_description, 0, sizeof(m_description));

	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false

	memset(m_dataFile, 0, sizeof(m_dataFile));
	memset(m_descriptionFile, 0, sizeof(m_descriptionFile));
}

CLStreamRecorder::~CLStreamRecorder()
{
	cleanup();
}

void CLStreamRecorder::cleanup()
{
    if (m_formatCtx) 
    {
        m_formatCtx->duration = 3600 * 1000;
        if (m_videoPacketWrited)
            av_write_trailer(m_formatCtx);
        for (int i = 0; i < m_formatCtx->nb_streams; ++i)
            avcodec_close(m_formatCtx->streams[i]->codec);
        avformat_free_context(m_formatCtx);
        m_formatCtx = 0;
    }
    m_videoPacketWrited = false;
    m_firstTimestamp = -1;

    if (m_iocontext)
        av_free(m_iocontext);
    m_iocontext = 0;

    if (m_outputFile)
    {
        m_outputFile->close();
        delete m_outputFile;
    }
    m_outputFile = 0;

	for (int i = 0; i < CL_MAX_CHANNELS; ++i)
	{
		delete m_data[i];
		m_data[i] = 0;

		delete m_description[i];
		m_description[i] = 0;

		m_gotKeyFrame[i] = false;

		if (m_dataFile[i])
		{
			fclose(m_dataFile[i]);
			m_dataFile[i] = 0;
		}

		if (m_descriptionFile[i])
		{
			fclose(m_descriptionFile[i]);
			m_descriptionFile[i] = 0;
		}
	}

	m_firstTime = true;

	memset(m_data, 0, sizeof(m_data));
	memset(m_description, 0, sizeof(m_description));

	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false

	memset(m_dataFile, 0, sizeof(m_dataFile));
	memset(m_descriptionFile, 0, sizeof(m_descriptionFile));
}

void CLStreamRecorder::correctH264Bitstream(AVPacket& packet)
{
    // replace short shart code to long start code 
    const quint8* end = packet.data + packet.size;
    const quint8* cur = NALUnit::findNextNAL(packet.data, end);
    m_tmpPacketBuffer.clear();
    m_tmpPacketBuffer.prepareToWrite(packet.size + 8);
    while (cur < end) {
        const quint8* next = NALUnit::findNALWithStartCode(cur+1, end, true);
        m_tmpPacketBuffer.write(FULL_NAL_PREFIX, sizeof(FULL_NAL_PREFIX));
        if (cur < next)
            m_tmpPacketBuffer.write((char*)cur, next - cur);
        cur = NALUnit::findNextNAL(cur+1, end);
    }
    packet.data = (quint8*) m_tmpPacketBuffer.data();
    packet.size = m_tmpPacketBuffer.size();
}

bool CLStreamRecorder::processData(CLAbstractData* data)
{

    if (m_firstTime)
    {
        m_firstTime = false;
        onFirstData(data);
    }

    CLAbstractMediaData* md = static_cast<CLAbstractMediaData*>(data);
    if (md->dataType != CLAbstractMediaData::VIDEO)
        return true;

    CLCompressedVideoData *vd= static_cast<CLCompressedVideoData*>(data);
    int channel = vd->channelNumber;


    if (channel>CL_MAX_CHANNELS-1)
        return true;

    bool keyFrame = vd->flags & AV_PKT_FLAG_KEY;

    if (keyFrame)
        m_gotKeyFrame[channel] = true;

    if (!m_gotKeyFrame[channel]) // did not got key frame so far
        return true;

    if (m_data[channel]==0)
        m_data[channel] = new CLByteArray(0, 1024*1024);

    if (m_description[channel]==0)
    {
        m_description[channel] = new CLByteArray(0, 1024);
        m_description[channel]->write((char*)(&m_version), 4); // version
    }

    AVPacket videoPkt;
    av_init_packet(&videoPkt);
    AVStream * stream = m_formatCtx->streams[channel];
    AVRational srcRate = {1, 1000000};
    if (m_firstTimestamp == -1)
        m_firstTimestamp = vd->timestamp;
    videoPkt.pts = av_rescale_q(vd->timestamp-m_firstTimestamp, srcRate, stream->time_base);
    if(keyFrame)
        videoPkt.flags |= AV_PKT_FLAG_KEY;
    videoPkt.stream_index= channel;
    videoPkt.data = (quint8*) vd->data.data();
    videoPkt.size = vd->data.size();
    if (stream->codec->codec_id == CODEC_ID_H264)
        correctH264Bitstream(videoPkt);

    if (av_write_frame(m_formatCtx, &videoPkt) < 0) {
        cl_log.log(QLatin1String("Video packet write error"), cl_logWARNING);
    }
    else {
        m_videoPacketWrited = true;
    }

    return true;
}

void CLStreamRecorder::endOfRun()
{
	cleanup();
}


struct FffmpegLog
{
    static void av_log_default_callback_impl(void* ptr, int level, const char* fmt, va_list vl)
    {
        Q_UNUSED(level)
            Q_UNUSED(ptr)
            Q_ASSERT(fmt && "NULL Pointer");

        if (!fmt) {
            return;
        }
        static char strText[1024 + 1];
        vsnprintf(&strText[0], sizeof(strText) - 1, fmt, vl);
        va_end(vl);

        qDebug() << "ffmpeg library: " << strText;
    }
};

int CLStreamRecorder::initIOContext()
{
    enum {
        MAX_READ_SIZE = 1024 * 4*2
    };

    struct IO_ffmpeg {
        static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
        {
            CLStreamRecorder* stream = reinterpret_cast<CLStreamRecorder *>(opaque);
            return stream ? stream->writePacketImpl(buf, bufSize) : 0;
        }
        static qint32 readPacket(void *opaque, quint8* buf, int size)
        {
            Q_ASSERT(opaque && "NULL Pointer");
            CLStreamRecorder* stream = reinterpret_cast<CLStreamRecorder *>(opaque);
            return stream ? stream->readPacketImpl(buf, size) : 0;
        }
        static qint64 seek(void* opaque, qint64 offset, qint32 whence)
        {
            Q_ASSERT(opaque && "NULL Pointer");
            CLStreamRecorder* stream = reinterpret_cast<CLStreamRecorder *>(opaque);
            return stream ? stream->seekPacketImpl(offset, whence) : 0;

        }
    };
    m_fileBuffer.resize(MAX_READ_SIZE);
    m_iocontext = av_alloc_put_byte(&m_fileBuffer[0], MAX_READ_SIZE, 1, this, &IO_ffmpeg::readPacket, &IO_ffmpeg::writePacket, &IO_ffmpeg::seek);
    return m_iocontext != 0;
}
qint32 CLStreamRecorder::writePacketImpl(quint8* buf, qint32 bufSize)
{
    Q_ASSERT(buf && "NULL Pointer");
    return m_outputFile ? m_outputFile->write(reinterpret_cast<const char *>(buf), bufSize) : 0;
}
qint32 CLStreamRecorder::readPacketImpl(quint8* buf, quint32 bufSize)
{
    Q_ASSERT(buf && "NULL Pointer");
    Q_UNUSED(bufSize);
    return 0;
}
qint64 CLStreamRecorder::seekPacketImpl(qint64 offset, qint32 whence)
{
    Q_UNUSED(whence);
    if (!m_outputFile) {
        return -1;
    }
    if (whence != SEEK_END && whence != SEEK_SET && whence != SEEK_CUR) {
        return -1;
    }
    if (SEEK_END == whence && -1 == offset) {
        return UINT_MAX;
    }
    if (SEEK_END == whence) {
        offset = m_outputFile->size() - offset;
        offset = m_outputFile->seek(offset) ? offset : -1;
        return offset;
    }
    if (SEEK_CUR == whence) {
        offset += m_outputFile->pos();
        offset = m_outputFile->seek(offset) ? offset : -1;
        return offset;
    }
    if (SEEK_SET == whence) {
        offset = m_outputFile->seek(offset) ? offset : -1;
        return offset;
    }
    return -1;
}

QFileInfo CLStreamRecorder::getTempFileName(const QString& deviceId)
{
    QDir directory(getTempRecordingDir());
    QStringList filter;
    filter << deviceId + QString(".*");
    QFileInfoList list = directory.entryInfoList(filter);
    if (list.isEmpty())
        return QString();
    else
        return list[0];
}

QFileInfoList CLStreamRecorder::getRecordedFiles(const QString& dir)
{
    QDir directory(dir);
    QStringList filter;
    filter << "*.*";
    return directory.entryInfoList(filter, QDir::Files);
}

bool CLStreamRecorder::initFfmpegContainer(CLCompressedVideoData* mediaData, CLDevice* dev)
{
    avcodec_init();
    av_register_all();
    av_log_set_callback(FffmpegLog::av_log_default_callback_impl);
    // allocate container
    QString fileName = dirHelper();
    /*
    if (mediaData->compressionType == CODEC_ID_MJPEG) 
        m_outputCtx = av_guess_format("matroska",NULL,NULL);
    else 
        m_outputCtx = av_guess_format("mpegts",NULL,NULL);
    */
    m_outputCtx = av_guess_format("matroska",NULL,NULL);

    if (m_outputCtx == 0)
    {
        cl_log.log("av_guess_format failed.", cl_logERROR);
        return false;
    }
    QString fileExt = QString(m_outputCtx->extensions).split(',')[0];
    fileName += QString(".") + fileExt;
        
    m_outputFile = new QFile(fileName);
    if (!m_outputFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cl_log.log("Can.t open output file for video recording.", cl_logERROR);
        return false;
    }
    m_formatCtx = avformat_alloc_context();
    initIOContext();
    m_formatCtx->pb = m_iocontext;
    m_outputCtx->video_codec = mediaData->compressionType;
    m_formatCtx->oformat = m_outputCtx;
    

    if (av_set_parameters(m_formatCtx, NULL) < 0) {
        cl_log.log("Can't initialize output format parameters for recording video camera", cl_logERROR);
        return false;
    }
    QString layoutStr = CLAVIStreamReader::serializeLayout(dev->getVideoLayout(0));
    av_metadata_set2(&m_formatCtx->metadata, "video_layout", layoutStr.toAscii().data(), 0);
    //m_primaryChannel = mediaData->channelNumber;
    //av_metadata_set2(&m_formatCtx->metadata, "primary_video", QString::number(m_primaryChannel).toAscii().data(), 0);

    const CLDeviceVideoLayout* layout = dev->getVideoLayout(0);
    for (unsigned int i = 0; i < layout->numberOfChannels(); ++i) 
    {
        
        AVStream* videoStream = av_new_stream(m_formatCtx, DEFAULT_VIDEO_STREAM_ID+i);
        if (videoStream == 0)
        {
            cl_log.log("Can't allocate output stream for video recording.", cl_logERROR);
            return false;
        }

        AVCodecContext* videoCodecCtx = videoStream->codec;
        videoCodecCtx->codec_id = m_outputCtx->video_codec;
        videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        AVRational defaultFrameRate = {1, 30};
        videoCodecCtx->time_base = defaultFrameRate;
        videoCodecCtx->width = mediaData->width;
        videoCodecCtx->height = mediaData->height;
        
        if (mediaData->compressionType == CODEC_ID_MJPEG)
            videoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
        else
            videoCodecCtx->pix_fmt = PIX_FMT_YUV420P;
        videoCodecCtx->bit_rate = 1000000 * 6;

        videoCodecCtx->sample_aspect_ratio.num = 1;
        videoCodecCtx->sample_aspect_ratio.den = 1;
        videoStream->sample_aspect_ratio = videoCodecCtx->sample_aspect_ratio;
        videoStream->first_dts = 0;

        QString layoutData = QString::number(layout->v_position(i)) + QString(",") + QString(layout->h_position(i));
        av_metadata_set2(&videoStream->metadata, "layout_channel", QString::number(i).toAscii().data(), 0);
    }

    av_write_header(m_formatCtx);
    return true;
}

void CLStreamRecorder::onFirstData(CLAbstractData* data)
{
    m_videoPacketWrited = false;
    CLAbstractMediaData* media = static_cast<CLAbstractMediaData*> (data);
    QDir current_dir;
    current_dir.mkpath(getTempRecordingDir());

    CLStreamreader* reader = data->dataProvider;
    CLDevice* dev = reader->getDevice();

    initFfmpegContainer(static_cast<CLCompressedVideoData*>(data), dev);
}

QString CLStreamRecorder::filenameData(int channel)
{
	return filenameHelper(channel) + QLatin1String(".data");
}

QString CLStreamRecorder::filenameDescription(int channel)
{
	return filenameHelper(channel) + QLatin1String(".descr");
}

QString CLStreamRecorder::filenameHelper(int channel)
{
	QString str;
	QTextStream stream(&str);

	stream << dirHelper() << "/channel_" << channel;

	return str;
}

QString CLStreamRecorder::dirHelper()
{
	QString str;
	QTextStream stream(&str);


	stream << getTempRecordingDir() << m_device->getUniqueId();
	return str;
}

