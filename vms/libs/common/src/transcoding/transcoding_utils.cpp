#include "transcoding_utils.h"

#include <array>

#include <nx/utils/log/log.h>
#include <nx/streaming/av_codec_media_context.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/h263_utils.h>
#include <utils/math/math.h>

namespace nx {
namespace transcoding {

namespace {

static const std::size_t kBufferSize = 1024 *32;
static const int kWritableBufferFlag = 1;
static const int kReadOnlyBufferFlag = 0;

constexpr int kWidthAlign = 16;
constexpr int kHeightAlign = 4;

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

AVCodecID findEncoderCodecId(const QString& codecName)
{
    QString codecLower = codecName.toLower();
    if (codecLower == "h263")
        codecLower = "h263p"; //< force using h263p codec

    AVCodec* avCodec = avcodec_find_encoder_by_name(codecLower.toUtf8().constData());
    if (avCodec)
        return avCodec->id;

    // Try to check codec substitutes if requested codecs not found.
    static std::map<std::string, std::vector<std::string>> codecSubstitutesMap
    {
        { "h264", {"libopenh264"} },
    };
    auto substitutes = codecSubstitutesMap.find(codecLower.toUtf8().constData());
    if (substitutes == codecSubstitutesMap.end())
        return AV_CODEC_ID_NONE;

    for(auto& substitute: substitutes->second)
    {
        avCodec = avcodec_find_encoder_by_name(substitute.c_str());
        if (avCodec)
            return avCodec->id;
    }
    return AV_CODEC_ID_NONE;
}

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
        QnFfmpegAvPacket pkt;

        AVStream *inputStream, *outputStream;

        if (av_read_frame(inputFormatContext, &pkt) < 0)
            break;

        inputStream = inputFormatContext->streams[pkt.stream_index];
        if (pkt.stream_index >= streamMappingSize ||
            streamMapping[pkt.stream_index] < 0)
        {
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
    }

    ret = av_write_trailer(outputFormatContext);

    qDebug() << "RETURN VALUE" << ret;

    return cleanUp(Error::noError);
}

QSize downscaleByHeight(const QSize& source, int newHeight)
{
    float ar = source.width() / (float)source.height();
    return QSize(newHeight * ar, newHeight);
}

QSize downscaleByWidth(const QSize& source, int newWidth)
{
    float ar = source.width() / (float)source.height();
    return QSize(newWidth, newWidth / ar);
}

QSize cropResolution(const QSize& source, const QSize& max)
{
    QSize result = source;
    if (result.height() > max.height())
        result = downscaleByHeight(result, max.height());

    if (result.width() > max.width())
        result = downscaleByWidth(result, max.width());

    return result;
}


QSize maxResolution(AVCodecID codec)
{
    using namespace nx::media_utils;
    struct CodecMaxSize
    {
        AVCodecID codec;
        QSize maxSize;
    };
    static const CodecMaxSize maxSizes[] =
    {
        { AV_CODEC_ID_H263P, QSize(h263::kMaxWidth, h263::kMaxHeight)},
        { AV_CODEC_ID_MJPEG, QSize(2048, 2048)},
    };

    for(auto& size: maxSizes)
    {
        if (size.codec == codec)
            return size.maxSize;
    }
    return QSize(8192, 8192);
}

QSize adjustCodecRestrictions(AVCodecID codec, const QSize& source)
{
    QSize result(source);
    QSize maxSize = maxResolution(codec);
    if (source.width() > maxSize.width() || source.height() > maxSize.height())
    {
        result = cropResolution(result, maxSize);
        result.setWidth(qPower2Round(result.width(), kWidthAlign));
        result.setHeight(qPower2Round(result.height(), kHeightAlign));
        NX_DEBUG(NX_SCOPE_TAG, "Codec '%1' does not support resolution %2, downscale to %3",
            avcodec_get_name(codec), source, result);
    }
    switch (codec)
    {
        case AV_CODEC_ID_MPEG2VIDEO:
            // Width or Height are not allowed to be multiples of 4096 (see ffmpeg sources)
            if ((result.width() & 0xFFF) == 0)
                result.setWidth(result.width() - 4);

            if ((result.height() & 0xFFF) == 0)
                result.setHeight(result.height() - 4);

            if (source != result)
            {
                NX_DEBUG(NX_SCOPE_TAG, "mpeg2video does not support resolution %1, change it to %2",
                    source, result);
            }
            return result;

        default:
            return result;
    }
}

QSize normalizeResolution(AVCodecID codec, const QSize& target, const QSize& source)
{
    QSize result;
    if (target.width() < 1 && target.height() < 1)
    {
        result = source;
    }
    else if (target.width() == 0 && target.height() > 0)
    {
        if (source.isEmpty())
            return QSize();

        // TODO is it correct?
        int height = qMin(target.height(), source.height()); // Limit by source size.

        // TODO is align needed for all codecs(may be h263 only)?
        height = qPower2Round(height, kHeightAlign); // Round resolution height.

        result = downscaleByHeight(source, height);
        result.setWidth(qPower2Round(result.width(), kWidthAlign));
    }
    else
    {
        result = target;
    }

    if (codec != AV_CODEC_ID_NONE)
        result = adjustCodecRestrictions(codec, result);

    return result;
}

} // namespace transcoding
} // namespace nx
