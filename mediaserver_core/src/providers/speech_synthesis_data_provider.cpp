#if !defined(EDGE_SERVER)
#include "speech_synthesis_data_provider.h"

#include <nx_speech_synthesizer/text_to_wav.h>
#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/resource/resource.h>
#include <QtCore/QBuffer>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>

namespace {
    const size_t kDefaultDataChunkSize = 2048;
}

QnSpeechSynthesisDataProvider::QnSpeechSynthesisDataProvider(const QString& text) :
    QnAbstractStreamDataProvider(QnResourcePtr(new QnResource())),
    m_text(text),
    m_curPos(0)
{
    m_ctx = initializeAudioContext();
}

QnSpeechSynthesisDataProvider::~QnSpeechSynthesisDataProvider()
{
    stop();
}

QnConstMediaContextPtr QnSpeechSynthesisDataProvider::initializeAudioContext()
{
    auto synthesizer = TextToWaveServer::instance();
    auto format = synthesizer->getAudioFormat();
    auto codecId = CODEC_ID_PCM_S16LE; // synthesizer->getCodecId();
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

QnAbstractMediaDataPtr QnSpeechSynthesisDataProvider::getNextData()
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
    packet->compressionType = CodecID::CODEC_ID_PCM_S16LE;
    packet->dataType = QnAbstractMediaData::DataType::AUDIO;
    packet->dataProvider = this;
    m_curPos += kDefaultDataChunkSize;

    return QnAbstractMediaDataPtr(packet);
}

void QnSpeechSynthesisDataProvider::run()
{
    m_rawBuffer = doSynthesis(m_text);

    while(auto data = getNextData())
    {
        if(dataCanBeAccepted())
            putData(data);
    }

    afterRun();
}

void QnSpeechSynthesisDataProvider::afterRun()
{
    QnAbstractMediaDataPtr endOfStream(new QnEmptyMediaData());
    endOfStream->dataProvider = this;
    putData(endOfStream);
}

void QnSpeechSynthesisDataProvider::setText(const QString &text)
{
    m_text = text;
}

QByteArray QnSpeechSynthesisDataProvider::doSynthesis(const QString &text)
{
    QBuffer soundBuf;
    soundBuf.open(QIODevice::WriteOnly);

#ifndef DISABLE_FESTIVAL
    auto ttvInstance = TextToWaveServer::instance();
    ttvInstance->generateSoundSync(text, &soundBuf);
#endif

    const size_t kSynthesizerOutputHeaderSize = 52;
    return soundBuf.data().mid(kSynthesizerOutputHeaderSize);
}

#endif


