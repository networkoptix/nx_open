#include "ffmpeg_audio_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <nx/utils/log/assert.h>
#include "utils/media/audio_processor.h"
#include "utils/media/ffmpeg_helper.h"

extern "C" {
#include <libavutil/opt.h>
}


namespace
{
    const std::chrono::microseconds kMaxAudioJitterUs = std::chrono::milliseconds(200);
    const int kAvcodecMaxAudioFrameSize = 192000; // 1 second of 48khz 32bit audio
    const int kDefaultFrameSize = 1536;

    int calcBits(quint64 value)
    {
        int result = 0;
        while (value)
        {
            if (value & 1)
                ++result;
            value >>= 1;
        }
        return result;
    }

    int getMaxAudioChannels(AVCodec* avCodec)
    {
        if (!avCodec->channel_layouts)
            return 1; // default value if unknown

        int result = 0;
        for (const uint64_t* layout = avCodec->channel_layouts; *layout; ++layout)
            result = qMax(result, calcBits(*layout));
        return result;
    }

    int getDefaultDstSampleRate(int srcSampleRate, AVCodec* avCodec)
    {
        int result = srcSampleRate;
        bool isPcmCodec = avCodec->id == AV_CODEC_ID_ADPCM_G726
            || avCodec->id == AV_CODEC_ID_PCM_MULAW
            || avCodec->id == AV_CODEC_ID_PCM_ALAW;

        if (isPcmCodec)
            result = 8000;
        else
            result = std::max(result, 16000);

        return result;
    }
}

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(AVCodecID codecId):
    QnAudioTranscoder(codecId),
    m_encoderCtx(0),
    m_decoderCtx(0),
    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_lastTimestamp(AV_NOPTS_VALUE),
    m_downmixAudio(false),
    m_frameNum(0),
    m_resampleCtx(0),
    m_sampleBuffers(nullptr),
    m_currentSampleCount(0),
    m_currentSampleBufferOffset(0),
    m_dstSampleRate(0),
    m_frameDecodeTo(av_frame_alloc()),
    m_frameToEncode(av_frame_alloc())
{
    m_bitrate = 128*1000;
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    if (m_resampleCtx)
        swr_free(&m_resampleCtx);

    QnFfmpegHelper::deleteAvCodecContext(m_encoderCtx);
    QnFfmpegHelper::deleteAvCodecContext(m_decoderCtx);
    
    av_frame_free(&m_frameDecodeTo);
    av_frame_free(&m_frameToEncode);

    //Segfault here, need to free it properly
    /*av_freep(m_sampleBuffers);
    delete[] m_sampleBuffersWithOffset;*/
}

bool QnFfmpegAudioTranscoder::open(const QnConstCompressedAudioDataPtr& audio)
{
    if (!audio->context)
    {
        m_lastErrMessage = tr("Audio context was not specified.");
        return false;
    }

    QnAudioTranscoder::open(audio);

    return open(audio->context);
}

bool QnFfmpegAudioTranscoder::open(const QnConstMediaContextPtr& context)
{
    NX_ASSERT(context);

    AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (avCodec == 0)
    {
        m_lastErrMessage = tr("Could not find encoder for codec %1.").arg(m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    //m_encoderCtx->codec_id = m_codecId;
    //m_encoderCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_encoderCtx->sample_fmt = avCodec->sample_fmts[0] != AV_SAMPLE_FMT_NONE ? avCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16;

    m_encoderCtx->channels = context->getChannels();
    int maxEncoderChannels = getMaxAudioChannels(avCodec);

    if (m_encoderCtx->channels > maxEncoderChannels)
    {
        if (maxEncoderChannels == 2)
            m_downmixAudio = true; //< downmix to stereo inplace before ffmpeg based resample (just not change behaviour, keep as in the previous version)
        m_encoderCtx->channels = maxEncoderChannels;
    }
    m_encoderCtx->sample_rate = context->getSampleRate();
    if (m_dstSampleRate > 0)
        m_encoderCtx->sample_rate = m_dstSampleRate;
    else
        m_encoderCtx->sample_rate = getDefaultDstSampleRate(context->getSampleRate(), avCodec);

    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = 64000 * m_encoderCtx->channels;


    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio encoder.");
        return false;
    }

    avCodec = avcodec_find_decoder(context->getCodecId());
    m_decoderCtx = avcodec_alloc_context3(0);
    QnFfmpegHelper::mediaContextToAvCodecContext(m_decoderCtx, context);
    if (avcodec_open2(m_decoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio decoder.");
        return false;
    }

    m_context = QnConstMediaContextPtr(new QnAvCodecMediaContext(m_encoderCtx));
    m_frameNum = 0;
    return true;
}

bool QnFfmpegAudioTranscoder::isOpened() const
{
    return m_context != nullptr;
}

bool QnFfmpegAudioTranscoder::existMoreData() const
{
    //int encoderFrameSize = m_encoderCtx->frame_size * QnFfmpegHelper::audioSampleSize(m_encoderCtx);
    qDebug() << "Checking if more data exists" << m_currentSampleCount << m_encoderCtx->frame_size;
    return m_currentSampleCount >= m_encoderCtx->frame_size;
}

/*int QnFfmpegAudioTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if( result )
        result->reset();

    if (!m_lastErrMessage.isEmpty())
        return -3;

    if (media) {
        if (qAbs(media->timestamp - m_lastTimestamp) > MAX_AUDIO_JITTER || m_lastTimestamp == AV_NOPTS_VALUE)
        {
            m_frameNum = 0;
            m_firstEncodedPts = media->timestamp;
        }

        m_lastTimestamp = media->timestamp;
        QnConstCompressedAudioDataPtr audio = std::dynamic_pointer_cast<const QnCompressedAudioData>(media);
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = const_cast<quint8*>((const quint8*)media->data());
        avpkt.size = static_cast<int>(media->dataSize());

        //int decodedAudioSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        // TODO: #vasilenko avoid using deprecated methods

        bool needResample=
            m_encoderCtx->channels != m_decoderCtx->channels ||
            m_encoderCtx->sample_rate != m_decoderCtx->sample_rate ||
            m_encoderCtx->sample_fmt != m_decoderCtx->sample_fmt;

        QnByteArray& bufferToDecode = needResample ? m_unresampledData : m_resampledData;
        quint8* decodedDataEndPtr = (quint8*) bufferToDecode.data() + bufferToDecode.size();

        // todo: ffmpeg-test
        int got_frame = 0;
        int decodeResult = avcodec_decode_audio4(m_decoderCtx, m_outFrame, &got_frame, &avpkt);
        int decodedAudioSize = m_outFrame->nb_samples * QnFfmpegHelper::audioSampleSize(m_decoderCtx);
        if (got_frame)
        {
            if (!m_audioHelper)
                m_audioHelper.reset(new QnFfmpegAudioHelper(m_decoderCtx));
            m_audioHelper->copyAudioSamples(decodedDataEndPtr, m_outFrame);
        }

        if (m_encoderCtx->frame_size == 0)
            m_encoderCtx->frame_size = m_decoderCtx->frame_size;

        if (decodeResult < 0)
            return -3;

        if (decodedAudioSize > 0)
        {
            if (m_downmixAudio)
                decodedAudioSize = QnAudioProcessor::downmix(decodedDataEndPtr, decodedAudioSize, m_decoderCtx);
            bufferToDecode.resize(bufferToDecode.size() + decodedAudioSize);

            if (needResample)
            {
                if (m_resampleCtx == 0)
                {
                    m_resampleCtx = av_audio_resample_init(
                        m_encoderCtx->channels,
                        m_decoderCtx->channels,
                        m_encoderCtx->sample_rate,
                        m_decoderCtx->sample_rate,
                        m_encoderCtx->sample_fmt,
                        av_get_packed_sample_fmt(m_decoderCtx->sample_fmt),
                        16, 10, 0, 0.8);
                }

                int inSamples = bufferToDecode.size() / QnFfmpegHelper::audioSampleSize(m_decoderCtx);
                int outSamlpes = audio_resample(
                    m_resampleCtx,
                    (short*) (m_resampledData.data() + m_resampledData.size()),
                    (short*) m_unresampledData.data(),
                    inSamples);
                int resampledDataBytes = outSamlpes * QnFfmpegHelper::audioSampleSize(m_encoderCtx);
                m_resampledData.resize(m_resampledData.size() + resampledDataBytes);
                m_unresampledData.clear();
            }
        }
        NX_ASSERT(m_resampledData.size() < AVCODEC_MAX_AUDIO_FRAME_SIZE);
    }

    if( !result )
    {
        m_resampledData.clear(); //< we asked to skip input data
        return 0;
    }

    int encoderFrameSize = m_encoderCtx->frame_size * QnFfmpegHelper::audioSampleSize(m_encoderCtx);

    int encoded = 0;
    // todo: ffmpeg-test
    while (encoded == 0 && (int64_t)m_resampledData.size() >= (int64_t)encoderFrameSize)
    {
        //encoded = avcodec_encode_audio(m_encoderCtx, m_audioEncodingBuffer, FF_MIN_BUFFER_SIZE, (const short*) m_resampledData.data());
        AVPacket outputPacket;
        av_init_packet(&outputPacket);
        outputPacket.data = m_audioEncodingBuffer;
        outputPacket.size = FF_MIN_BUFFER_SIZE;

        m_inputFrame->data[0] = (quint8*) m_resampledData.data();
        m_inputFrame->nb_samples = m_encoderCtx->frame_size;

        int got_packet = 0;

        if (avcodec_encode_audio2(m_encoderCtx, &outputPacket, m_inputFrame, &got_packet) < 0)
            return -3; //< TODO: needs refactor. add enum with error codes
        if (got_packet)
            encoded = outputPacket.size;
        int resampledBufferRest = m_resampledData.size() - encoderFrameSize;
        memmove(m_resampledData.data(), m_resampledData.data() + encoderFrameSize, resampledBufferRest);
        m_resampledData.resize(resampledBufferRest);
    }

    if (encoded == 0)
        return 0;


    QnWritableCompressedAudioData* resultAudioPacket = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, encoded, m_context);
    resultAudioPacket->compressionType = m_codecId;
    static AVRational r = {1, 1000000};
    //result->timestamp  = m_lastTimestamp;
    qint64 audioPts = m_frameNum++ * m_encoderCtx->frame_size;
    resultAudioPacket->timestamp  = av_rescale_q(audioPts, m_encoderCtx->time_base, r) + m_firstEncodedPts;
    resultAudioPacket->m_data.write((const char*) m_audioEncodingBuffer, encoded);
    *result = QnCompressedAudioDataPtr(resultAudioPacket);

    return 0;
}*/

int QnFfmpegAudioTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if (result)
        result->reset();

    // push media to decoder
    if (media)
    {
        if (media->dataType != QnAbstractMediaData::DataType::AUDIO)
            return 0;

        const std::chrono::microseconds kTimestampDiffUs(media->timestamp - m_lastTimestamp);
        const bool tooBigJitter = std::abs(kTimestampDiffUs.count()) > kMaxAudioJitterUs.count()
            || m_lastTimestamp == AV_NOPTS_VALUE;

        if (tooBigJitter)
        {
            m_frameNum = 0;
            m_firstEncodedPts = media->timestamp;
        }

        m_lastTimestamp = media->timestamp;

        AVPacket packetToDecode;
        av_init_packet(&packetToDecode);

        packetToDecode.data = const_cast<quint8*>((const quint8*)media->data());
        packetToDecode.size = static_cast<int>(media->dataSize());

        {
            //LOGGING

            static QFile* preDecodedFile = nullptr;

            if (!preDecodedFile)
            {
                preDecodedFile = new QFile(lit("C:\\media_logs\\axis.raw"));
                preDecodedFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
            }

            QByteArray preDecodedData((char*)packetToDecode.data, packetToDecode.size);
            preDecodedFile->write(preDecodedData);
        }

        qDebug() << lit("GOT MEDIA DATA! SIZE: %1").arg(packetToDecode.size);

        auto resultOfSend = avcodec_send_packet(m_decoderCtx, &packetToDecode);

        NX_ASSERT(
            resultOfSend != AVERROR(EAGAIN),
            lit("Wrong transcoder usage: decoder can't receive more data, pull all the encoded packets first"));

        if (resultOfSend != 0)
            return resultOfSend;
    }

    //These're very important lines, don't touch it
    if (m_decoderCtx->frame_size == 0)
        m_decoderCtx->frame_size = kDefaultFrameSize;

    if (!m_decoderCtx->channel_layout)
        m_decoderCtx->channel_layout = AV_CH_LAYOUT_MONO;

    if (m_encoderCtx->frame_size == 0)
        m_encoderCtx->frame_size = m_decoderCtx->frame_size;

    if (m_encoderCtx->channels == 0)
        m_encoderCtx->channels = 1;

    if (m_encoderCtx->channel_layout == 0)
    {
        if (media->context)
            m_encoderCtx->channel_layout = media->context->getChannelLayout();
    }

    if (m_encoderCtx->channel_layout == 0)
    {
        if (m_encoderCtx->channels == 2)
            m_encoderCtx->channel_layout = AV_CH_LAYOUT_STEREO;
        else
            m_encoderCtx->channel_layout = AV_CH_LAYOUT_MONO;
    }
        

    // It's wrong, fix it
    static bool reinitialized = false;
    if (!reinitialized)
    {
        m_context = QnConstMediaContextPtr(new QnAvCodecMediaContext(m_encoderCtx));
        reinitialized = true;
    }

    initResampleCtx(m_decoderCtx, m_encoderCtx);
    allocSampleBuffers(m_decoderCtx, m_encoderCtx, m_resampleCtx);

    while (true)
    {
        // first we try to fill the frame from existing samples
        if (m_currentSampleCount >= m_encoderCtx->frame_size)
        {
            //TODO: #dmishin this function likely doesn't work properly, fix it
            fillFrameWithSamples(
                m_frameToEncode,
                m_sampleBuffers,
                m_currentSampleBufferOffset,
                m_encoderCtx);

            m_currentSampleBufferOffset += m_encoderCtx->frame_size;
            m_currentSampleCount -= m_encoderCtx->frame_size;

            auto resultOfSend = avcodec_send_frame(m_encoderCtx, m_frameToEncode);

            AVPacket packetEncodeTo;
            av_init_packet(&packetEncodeTo);

            auto resultOfReceive = avcodec_receive_packet(m_encoderCtx, &packetEncodeTo);

            // Not enough data to encode packet.
            if (resultOfReceive == AVERROR(EAGAIN))
                continue;

            if (resultOfReceive != 0)
                return resultOfReceive;

            {
                //LOGGING
                static QFile* encodedFile = nullptr;

                if (!encodedFile)
                {
                    encodedFile = new QFile(lit("C:\\media_logs\\encoded.vorbis"));
                    encodedFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
                }

                QByteArray encodedData((char*)packetEncodeTo.data, packetEncodeTo.size);
                encodedFile->write(encodedData);
            }

            *result = createMediaDataFromAVPacket(packetEncodeTo);
            return 0;
        }

        shiftSamplesResidue(
            m_encoderCtx,
            m_sampleBuffers,
            m_currentSampleBufferOffset,
            m_currentSampleCount);

        m_currentSampleBufferOffset = 0;

        auto resultOfReceive = avcodec_receive_frame(m_decoderCtx, m_frameDecodeTo);

        qDebug() << "Result of receive" << resultOfReceive << m_frameDecodeTo->nb_samples;

        /*if (resultOfReceive == 0)
        {
            //LOGGING
            static QFile* decodedFile = nullptr; 
            static QFile* decodedFile2 = nullptr; 
            
            if (!decodedFile)
            {
                decodedFile = new QFile(lit("C:\\media_logs\\decoded.raw"));
                decodedFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
            }

            if (!decodedFile2)
            {
                decodedFile2 = new QFile(lit("C:\\media_logs\\decoded2.raw"));
                decodedFile2->open(QIODevice::WriteOnly | QIODevice::Truncate);
            }
            
            QByteArray decodedData((char*)m_frameDecodeTo->extended_data[0], m_frameDecodeTo->nb_samples * av_get_bytes_per_sample(m_decoderCtx->sample_fmt));
            QByteArray decodedData2((char*)m_frameDecodeTo->extended_data[1], m_frameDecodeTo->nb_samples * av_get_bytes_per_sample(m_decoderCtx->sample_fmt));
            decodedFile->write(decodedData);
            decodedFile2->write(decodedData2);
            decodedFile->flush();
            decodedFile2->flush();
        }*/


        // There is not enough data to decode
        if (resultOfReceive == AVERROR(EAGAIN))
            return 0;

        if (resultOfReceive != 0)
            return resultOfReceive;

        auto resampleStatus = doResample();

        if (resampleStatus < 0)
            return resampleStatus;
    }

    return 0;
}

AVCodecContext* QnFfmpegAudioTranscoder::getCodecContext()
{
    return m_encoderCtx;
}

void QnFfmpegAudioTranscoder::setSampleRate(int value)
{
    m_dstSampleRate = value;
}

void QnFfmpegAudioTranscoder::initResampleCtx(const AVCodecContext *inCtx, const AVCodecContext *outCtx)
{
    if (m_resampleCtx)
        return;

    m_resampleCtx = swr_alloc();

    av_opt_set_channel_layout(
        m_resampleCtx,
        "in_channel_layout",
        inCtx->channel_layout,
        0);

    av_opt_set_channel_layout(
        m_resampleCtx,
        "out_channel_layout",
        outCtx->channel_layout,
        0);

    av_opt_set_int(m_resampleCtx, "in_sample_rate", inCtx->sample_rate, 0);
    av_opt_set_int(m_resampleCtx, "out_sample_rate", outCtx->sample_rate, 0);
    av_opt_set_sample_fmt(m_resampleCtx, "in_sample_fmt", inCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(m_resampleCtx, "out_sample_fmt", outCtx->sample_fmt, 0);

    qDebug() << "INITIALIZING SWR CONTEXT" << swr_init(m_resampleCtx);

    const int kBufSize = 200;
    char inChStr[kBufSize];
    char outChStr[kBufSize];
    char inFmt[kBufSize];
    char outFmt[kBufSize];

    av_get_channel_layout_string(inChStr, kBufSize, inCtx->channels, inCtx->channel_layout);
    av_get_channel_layout_string(outChStr, kBufSize, outCtx->channels, outCtx->channel_layout);
    av_get_sample_fmt_string(inFmt, kBufSize, inCtx->sample_fmt);
    av_get_sample_fmt_string(outFmt, kBufSize, outCtx->sample_fmt);

    qDebug() << lit(
        "Audio transcoder, initializing resampling context. Resample context is initialized with the folowing parameters:"
        "in_channel_layout: %1,"
        "out_channel_layout: %2,"
        "in_sample_rate: %3,"
        "out_sample_rate: %4,"
        "in_sample_fmt: %5,"
        "out_sample_fmt %6")
            .arg(QLatin1String(inChStr))
            .arg(QLatin1String(outChStr))
            .arg(inCtx->sample_rate)
            .arg(outCtx->sample_rate)
            .arg(QLatin1String(inFmt))
            .arg(QLatin1String(outFmt));

}

void QnFfmpegAudioTranscoder::allocSampleBuffers(
    const AVCodecContext *inCtx,
    const AVCodecContext *outCtx,
    const SwrContext* resampleCtx)
{
    if (m_sampleBuffers)
        return;

    auto buffersCount = av_sample_fmt_is_planar(outCtx->sample_fmt) ? outCtx->channels : 1;

    //get output sample count after resampling of a single frame
    auto outSampleCount = av_rescale_rnd(
        swr_get_delay(
            const_cast<SwrContext*>(resampleCtx),
            inCtx->sample_rate) + inCtx->frame_size,
        outCtx->sample_rate,
        inCtx->sample_rate,
        AV_ROUND_UP);

    qDebug() << lit("Audio transcoder, allocating buffers. Calculated output sample count: %1")
        .arg(outSampleCount);

    //TODO: #dmishin think about this expression one more time.
    std::size_t bufferSize = outSampleCount + outCtx->frame_size;    

    int linesize = 0;
    const int kDefaultAlign = 0;

    auto result = av_samples_alloc_array_and_samples(
        &m_sampleBuffers,
        &linesize,
        outCtx->channels,
        bufferSize,
        outCtx->sample_fmt,
        kDefaultAlign);

    m_sampleBuffersWithOffset = new uint8_t*[buffersCount];
    for (std::size_t i = 0; i < buffersCount; ++i)
        m_sampleBuffersWithOffset[i] = m_sampleBuffers[i];


    qDebug() <<
        lit("Audio transcoder, allocating buffers. Sample buffers allocated for %1 channels, size of single channel buffer %2, linesize %3")
            .arg(outCtx->channels)
            .arg(bufferSize)
            .arg(linesize);
}

void QnFfmpegAudioTranscoder::shiftSamplesResidue(
    const AVCodecContext* ctx,
    uint8_t* const* const buffersBase,
    const std::size_t samplesOffset,
    const std::size_t samplesLeftPerChannel)
{
    qDebug() << lit("Audio transcoder, shifting samples: samples offset %1, samples left per channel %2")
        .arg(samplesOffset)
        .arg(samplesLeftPerChannel);

    if (!samplesOffset)
        return;

    bool isPlanar = av_sample_fmt_is_planar(ctx->sample_fmt);
    std::size_t buffersCount = isPlanar ?
        ctx->channels : 1;

    if (!samplesLeftPerChannel)
    {
        for (std::size_t bufferNum = 0; bufferNum < buffersCount; bufferNum++)
            m_sampleBuffersWithOffset[bufferNum] = buffersBase[bufferNum];

        return;
    }

    std::size_t bytesNumberToCopy = av_get_bytes_per_sample(ctx->sample_fmt) 
        * samplesLeftPerChannel
        * (isPlanar ? 1 : ctx->channels);

    qDebug() << lit("Audio transcoder, shifting channels: format is planar? %1, buffer count: %2, bytes to copy: %3")
        .arg(isPlanar ? lit("YES") : lit("NO"))
        .arg(buffersCount)
        .arg(bytesNumberToCopy);

    for (std::size_t bufferNum = 0; bufferNum < buffersCount; bufferNum++)
    {
        memmove(buffersBase[bufferNum], buffersBase[bufferNum] + samplesOffset * av_get_bytes_per_sample(ctx->sample_fmt), bytesNumberToCopy);
        m_sampleBuffersWithOffset[bufferNum] = buffersBase[bufferNum] + bytesNumberToCopy;
    }

}

void QnFfmpegAudioTranscoder::fillFrameWithSamples(
    AVFrame* frame,
    uint8_t** sampleBuffers,
    const std::size_t offset,
    const AVCodecContext* encoderCtx)
{
    bool isPlanar = av_sample_fmt_is_planar(encoderCtx->sample_fmt);
    auto planesCount = isPlanar ?
        encoderCtx->channels : 1;

    for (auto i = 0; i < planesCount; ++i)
    {
        frame->data[i] = sampleBuffers[i] + 
            offset 
                * av_get_bytes_per_sample(encoderCtx->sample_fmt)
                * (isPlanar ? 1 : encoderCtx->channels);
    }

    frame->extended_data = frame->data;
    frame->nb_samples = encoderCtx->frame_size;
    frame->sample_rate = encoderCtx->sample_rate;

   /* qDebug() << "Frame number of samples" << frame->nb_samples << encoderCtx->frame_size;

    {
        //LOGGING
        static QFile* resampledFile = nullptr;
        static QFile* resampledFile2 = nullptr;

        if (!resampledFile)
        {
            resampledFile = new QFile(lit("C:\\media_logs\\resampled.raw"));
            resampledFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
        }

        if (!resampledFile2)
        {
            resampledFile2 = new QFile(lit("C:\\media_logs\\resampled2.raw"));
            resampledFile2->open(QIODevice::WriteOnly | QIODevice::Truncate);
        }

        QByteArray preDecodedData((char*)frame->extended_data[0], frame->nb_samples * av_get_bytes_per_sample(m_encoderCtx->sample_fmt));
        QByteArray preDecodedData2((char*)frame->extended_data[1], frame->nb_samples * av_get_bytes_per_sample(m_encoderCtx->sample_fmt));
        resampledFile->write(preDecodedData);
        resampledFile2->write(preDecodedData2);
    }*/
}

int QnFfmpegAudioTranscoder::doResample()
{
    const auto kLinesize = 3200;
    //TODO: #dmishin restore assert then
    /*NX_ASSERT(
        m_sampleBuffersWithOffset[0] - m_sampleBuffers[0] + 418 * 2 <= kLinesize, 
        lit("ASSERT OVERFLOW %1").arg(m_sampleBuffersWithOffset[0] - m_sampleBuffers[0] + 418 * 2));*/

    auto outSampleCount = av_rescale_rnd(
        swr_get_delay(
            const_cast<SwrContext*>(m_resampleCtx),
            m_decoderCtx->sample_rate) + m_decoderCtx->frame_size,
        m_encoderCtx->sample_rate,
        m_decoderCtx->sample_rate,
        AV_ROUND_UP);

    auto outSamplesPerChannel = swr_convert(
        m_resampleCtx,
        m_sampleBuffersWithOffset, //out bufs
        outSampleCount,//m_encoderCtx->frame_size, //out size in sample
        const_cast<const uint8_t**>(m_frameDecodeTo->extended_data), //in bufs
        m_frameDecodeTo->nb_samples); // in size in samples

    

    qDebug() << "Audio transcoder, resampled samples per channel" << outSamplesPerChannel;

    if (outSamplesPerChannel < 0)
        return outSamplesPerChannel;

    /*{
        //LOGGING
        static QFile* AresampledFile = nullptr;
        static QFile* AresampledFile2 = nullptr;

        if (!AresampledFile)
        {
            AresampledFile = new QFile(lit("C:\\media_logs\\Aresampled.raw"));
            AresampledFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
        }

        if (!AresampledFile2)
        {
            AresampledFile2 = new QFile(lit("C:\\media_logs\\Aresampled2.raw"));
            AresampledFile2->open(QIODevice::WriteOnly | QIODevice::Truncate);
        }

        QByteArray ApreDecodedData((char*)m_sampleBuffersWithOffset[0], outSamplesPerChannel * av_get_bytes_per_sample(m_encoderCtx->sample_fmt));
        QByteArray ApreDecodedData2((char*)m_sampleBuffersWithOffset[1], outSamplesPerChannel * av_get_bytes_per_sample(m_encoderCtx->sample_fmt));
        AresampledFile->write(ApreDecodedData);
        AresampledFile2->write(ApreDecodedData2);
    }*/

    m_currentSampleCount += outSamplesPerChannel;

    bool isPlanar = av_sample_fmt_is_planar(m_encoderCtx->sample_fmt);
    std::size_t buffersCount = isPlanar ? m_encoderCtx->channels : 1;

    for (std::size_t bufferNum = 0; bufferNum < buffersCount; ++bufferNum)
    {
        m_sampleBuffersWithOffset[bufferNum] += outSamplesPerChannel 
            * av_get_bytes_per_sample(m_encoderCtx->sample_fmt)
            * (isPlanar ? 1 : m_encoderCtx->channels);
    }

    return 0;
}

QnAbstractMediaDataPtr QnFfmpegAudioTranscoder::createMediaDataFromAVPacket(const AVPacket &packet)
{
    auto resultAudioData = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size, m_context);
    resultAudioData->compressionType = m_codecId;

    AVRational r = {1, 1000000};
    qint64 audioPts = m_frameNum++ * m_encoderCtx->frame_size;
    resultAudioData->timestamp  = av_rescale_q(audioPts, m_encoderCtx->time_base, r) + m_firstEncodedPts;

    qDebug() << "Audio transcoder, writing compressed audio data" << packet.size;
    resultAudioData->m_data.write((const char*)packet.data, packet.size);
    return  QnCompressedAudioDataPtr(resultAudioData);
}

#endif // ENABLE_DATA_PROVIDERS
