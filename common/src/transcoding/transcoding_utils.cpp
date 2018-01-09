#include "transcoding_utils.h"

#include <nx/utils/log/log.h>
#include <nx/streaming/av_codec_media_context.h>


namespace nx {
namespace transcoding {

namespace {

static const std::size_t kBufferSize = 1024 *32;
static const int kWritableBufferFlag = 1;
static const int kReadOnlyBufferFlag = 0;

int readFromQIODevice(void* opaque, uint8_t* buf, int size)
{

    auto ioDevice = reinterpret_cast<QIODevice*>(opaque);
    auto result = ioDevice->read((char*)buf, size);

    //qDebug() << "READING FROM IO DEVICE" << result;

    return result;
}

int writeToQIODevice(void *opaque, uint8_t* buf, int size)
{
    //qDebug() << "WRITING TO DEVICE";
    auto ioDevice = reinterpret_cast<QIODevice*>(opaque);
    return ioDevice->write((char*)buf, size);
}

} // namespace

Helper::Helper(QIODevice* inputMedia):
    m_inputMedia(inputMedia),
    m_inputAvioContext(nullptr),
    m_inputFormatContext(nullptr)
{
}

Helper::~Helper()
{
    close();
}

bool Helper::open()
{
    if (m_inputMedia->isOpen())
        m_inputMedia->open(QIODevice::ReadOnly);

    auto buffer = (unsigned char*)av_malloc(kBufferSize);
    auto avioContext = avio_alloc_context(
        buffer,
        kBufferSize,
        kReadOnlyBufferFlag,
        m_inputMedia,
        readFromQIODevice,
        writeToQIODevice,
        nullptr);

    m_inputFormatContext = avformat_alloc_context();

    if (!m_inputFormatContext)
        return false;

    m_inputFormatContext->pb = avioContext;

    if (avformat_open_input(&m_inputFormatContext, "dummy", nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(m_inputFormatContext, 0) < 0)
        return false;

    return true;
}

void Helper::close()
{
    avformat_close_input(&m_inputFormatContext);
    m_inputFormatContext = nullptr;

    if (m_inputAvioContext)
    {
        av_freep(&m_inputAvioContext->buffer);
        av_freep(&m_inputAvioContext);
    }

    if (!m_inputMedia->isSequential())
        m_inputMedia->reset();
}

int64_t Helper::durationMs()
{
    if (!m_inputFormatContext)
        return AV_NOPTS_VALUE;

    // Potential overflow here
    return av_rescale(m_inputFormatContext->duration, 1, AV_TIME_BASE) * 1000;
}

QnResourceAudioLayoutPtr Helper::audioLayout()
{
    if (!m_inputFormatContext)
        return QnResourceAudioLayoutPtr();

    for (auto i = 0; i < m_inputFormatContext->nb_streams; i++)
    {
        auto stream = m_inputFormatContext->streams[i];
        auto codecParameters = stream->codecpar;
        if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            QnResourceCustomAudioLayoutPtr layout(new QnResourceCustomAudioLayout());
            const auto context = new QnAvCodecMediaContext(codecParameters->codec_id);
            auto mediaContext = QnConstMediaContextPtr(context);
            const auto av = context->getAvCodecContext();
            av->channels = 1; //< TODO: #dmishin add support for multichannel audio
            av->sample_rate = codecParameters->sample_rate;
            av->sample_fmt = (AVSampleFormat)codecParameters->format;
            av->time_base.num = 1;
            av->bits_per_coded_sample = codecParameters->bits_per_coded_sample;
            av->time_base.den = codecParameters->sample_rate;

            QnResourceAudioLayout::AudioTrack track;
            track.codecContext = mediaContext;
            layout->addAudioTrack(track);

            return layout;
        }
    }

    return QnResourceAudioLayoutPtr();
}

QSize Helper::resolution()
{
    if (!m_inputFormatContext)
        return QSize();

    for (auto i = 0; i < m_inputFormatContext->nb_streams; i++)
    {
        auto stream = m_inputFormatContext->streams[i];
        auto codecParameters = stream->codecpar;
        if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO)
            return QSize(codecParameters->width, codecParameters->height);
    }

    return QSize();
}

Error remux(QIODevice* inputMedia, QIODevice* outputMedia, const QString& dstContainer)
{
    AVOutputFormat* outputFormat = nullptr;
    AVFormatContext* inputFormatContext = nullptr;
    AVFormatContext* outputFormatContext = nullptr;

    unsigned char* inputBuffer = nullptr;
    unsigned char* outputBuffer = nullptr;

    AVIOContext* inputAvioContext = nullptr;
    AVIOContext* outputAvioContext = nullptr;

    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int streamIndex = 0;
    int* streamMapping = nullptr;
    int streamMappingSize = 0;

    in_filename = "dummy_in";
    out_filename = "dummy_out";

    auto cleanUp =
        [&](Error returnCode) -> Error
        {
            avformat_close_input(&inputFormatContext);

            if (outputFormatContext)
                avformat_free_context(outputFormatContext);

            av_freep(&inputAvioContext->buffer);
            av_freep(&inputAvioContext);
            av_freep(&outputAvioContext->buffer);
            av_freep(&outputAvioContext);

            av_freep(&streamMapping);

            return returnCode;
        };

    inputBuffer = (unsigned char*)av_malloc(kBufferSize);
    if (!inputBuffer)
        return cleanUp(Error::badAlloc);

    inputAvioContext = avio_alloc_context(
        inputBuffer,
        kBufferSize,
        kReadOnlyBufferFlag,
        inputMedia,
        readFromQIODevice,
        NULL,
        NULL);

    if (!inputAvioContext)
        return cleanUp(Error::badAlloc);

    outputBuffer = (unsigned char*)av_malloc(kBufferSize);
    if (!outputBuffer)
        return cleanUp(Error::badAlloc);

    outputAvioContext = avio_alloc_context(
        outputBuffer,
        kBufferSize,
        kWritableBufferFlag,
        outputMedia,
        nullptr,
        writeToQIODevice,
        nullptr);

    inputFormatContext = avformat_alloc_context();
    inputFormatContext->pb = inputAvioContext;

    if (avformat_open_input(&inputFormatContext, "dummy", 0, 0) < 0)
        return cleanUp(Error::unableToOpenInput);

    if (avformat_find_stream_info(inputFormatContext, 0) < 0)
        return cleanUp(Error::noStreamInfo);

    qDebug() << "DURATION" << inputFormatContext->duration;

    av_dump_format(inputFormatContext, 0, in_filename, 0);

    avformat_alloc_output_context2(
        &outputFormatContext,
        NULL, dstContainer.toStdString().c_str(),
        NULL);

    if (!outputFormatContext)
        return cleanUp(Error::unknown);

    outputFormatContext->pb = outputAvioContext;

    streamMappingSize = inputFormatContext->nb_streams;
    streamMapping = (int*)av_mallocz_array(streamMappingSize, sizeof(*streamMapping));
    if (!streamMapping)
        return cleanUp(Error::badAlloc);

    outputFormat = outputFormatContext->oformat;

    for (i = 0; i < inputFormatContext->nb_streams; i++)
    {
        AVStream *outputStream;
        AVStream *inputStream = inputFormatContext->streams[i];
        AVCodecParameters *inputCodecParameters = inputStream->codecpar;

        if (inputCodecParameters->codec_type != AVMEDIA_TYPE_AUDIO &&
            inputCodecParameters->codec_type != AVMEDIA_TYPE_VIDEO &&
            inputCodecParameters->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            streamMapping[i] = -1;
            continue;
        }

        streamMapping[i] = streamIndex++;

        outputStream = avformat_new_stream(outputFormatContext, NULL);
        if (!outputStream)
            return cleanUp(Error::unknown);

        if (avcodec_parameters_copy(outputStream->codecpar, inputCodecParameters) < 0)
            return Error::unknown;

        /*outputStream->codec->extradata = inputStream->codec->extradata;
        outputStream->codec->extradata_size = inputStream->codec->extradata_size;*/

        outputStream->codecpar->codec_tag = 0;
    }

    av_dump_format(outputFormatContext, 0, out_filename, 1);

    if (avformat_write_header(outputFormatContext, NULL) < 0)
        return cleanUp(Error::unknown);

    while (1)
    {
        AVStream *inputStream, *outputStream;

        if (av_read_frame(inputFormatContext, &pkt) < 0)
            break;

        inputStream = inputFormatContext->streams[pkt.stream_index];
        if (pkt.stream_index >= streamMappingSize ||
            streamMapping[pkt.stream_index] < 0)
        {
            av_packet_unref(&pkt);
            continue;
        }

        pkt.stream_index = streamMapping[pkt.stream_index];
        outputStream = outputFormatContext->streams[pkt.stream_index];

        /* copy packet */
        pkt.pts = av_rescale_q_rnd(
            pkt.pts,
            inputStream->time_base,
            outputStream->time_base,
            (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

        pkt.dts = av_rescale_q_rnd(
            pkt.dts,
            inputStream->time_base,
            outputStream->time_base,
            (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

        if (pkt.pts < pkt.dts)
        {
            pkt.pts = pkt.dts;
        }

        pkt.flags |= AV_PKT_FLAG_KEY;

        pkt.duration = av_rescale_q(pkt.duration, inputStream->time_base, outputStream->time_base);
        pkt.pos = -1;

        //qDebug() << "PTS/DTS/DURATION" << pkt.pts << pkt.dts << pkt.duration;

        if (av_interleaved_write_frame(outputFormatContext, &pkt) < 0)
            break;

        av_packet_unref(&pkt);
    }

    ret = av_write_trailer(outputFormatContext);

    qDebug() << "RETURN VALUE" << ret;

    return cleanUp(Error::noError);
}

} // namespace transcoding
} // namespace nx