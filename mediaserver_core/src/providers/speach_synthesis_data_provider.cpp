#if !defined(EDGE_SERVER)

#include <nx_speach_synthesizer/text_to_wav.h>
#include "speach_synthesis_data_provider.h"
#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/resource/resource.h>
#include <QtCore/QBuffer>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/streaming/av_codec_media_context.h>

namespace {
    const size_t kDefaultDataChunkSize = 2048;
}

QnSpeachSynthesisDataProvider::QnSpeachSynthesisDataProvider(const QString& text) :
    QnAbstractStreamDataProvider(QnResourcePtr(new QnResource())),
    m_text(text),
    m_curPos(0)
{
    m_ctx = initializeAudioContext();
}

QnSpeachSynthesisDataProvider::~QnSpeachSynthesisDataProvider()
{
    stop();
}

QnConstMediaContextPtr QnSpeachSynthesisDataProvider::initializeAudioContext()
{
    auto synthesizer = TextToWaveServer::instance();
    auto format = synthesizer->getAudioFormat();
    auto codecId = AV_CODEC_ID_PCM_S16LE; // synthesizer->getCodecId();
    auto codec = avcodec_find_decoder(codecId);

    if (!codec)
        return QnConstMediaContextPtr();

    auto ctx = avcodec_alloc_context3(codec);

    if (!ctx)
        return QnConstMediaContextPtr();

    ctx->codec = codec;
    ctx->codec_id = codecId;
    ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ctx->sample_fmt = QnFfmpegAudioDecoder::audioFormatQtToFfmpeg(format);
    ctx->sample_rate = format.sampleRate();
    ctx->frame_size = kDefaultDataChunkSize / 2;
    ctx->channels = format.channelCount();

    return QnConstMediaContextPtr(new QnAvCodecMediaContext(ctx));
}

QnAbstractMediaDataPtr QnSpeachSynthesisDataProvider::getNextData()
{
    if (m_curPos >= m_rawBuffer.size())
        return QnAbstractMediaDataPtr();

    QnWritableCompressedAudioDataPtr packet(
        new QnWritableCompressedAudioData(
            CL_MEDIA_ALIGNMENT,
            kDefaultDataChunkSize,
            m_ctx));

    auto bytesRest = m_rawBuffer.size() - m_curPos;
    packet->m_data.write(
        m_rawBuffer.constData() + m_curPos,
        bytesRest < kDefaultDataChunkSize ? bytesRest : kDefaultDataChunkSize);
    packet->compressionType = AVCodecID::AV_CODEC_ID_PCM_S16LE;
    packet->dataType = QnAbstractMediaData::DataType::AUDIO;
    packet->dataProvider = this;
    m_curPos += kDefaultDataChunkSize;

    return QnAbstractMediaDataPtr(packet);
}

void QnSpeachSynthesisDataProvider::run()
{
    m_rawBuffer = doSynthesis(m_text);

    while(auto data = getNextData())
    {
        if(dataCanBeAccepted())
            putData(data);
    }

    afterRun();
}

void QnSpeachSynthesisDataProvider::afterRun()
{
    QnAbstractMediaDataPtr endOfStream(new QnEmptyMediaData());
    endOfStream->dataProvider = this;
    putData(endOfStream);
}

void QnSpeachSynthesisDataProvider::setText(const QString &text)
{
    m_text = text;
}

QByteArray QnSpeachSynthesisDataProvider::doSynthesis(const QString &text)
{
    QBuffer soundBuf;
    soundBuf.open(QIODevice::WriteOnly);
    auto ttvInstance = TextToWaveServer::instance();
    const size_t kSynthesizerOutputHeaderSize = 52;

    ttvInstance->generateSoundSync(text, &soundBuf);
    return soundBuf.data().mid(kSynthesizerOutputHeaderSize);
}

#endif


