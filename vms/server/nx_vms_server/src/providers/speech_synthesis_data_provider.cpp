#include "speech_synthesis_data_provider.h"

#include <nx/speech_synthesizer/text_to_wave_server.h>
#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/resource/resource.h>
#include <QtCore/QBuffer>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>

static const int kDefaultDataChunkSize = 2048;

/*static*/ bool QnSpeechSynthesisDataProvider::isEnabled()
{
    return nx::speech_synthesizer::TextToWaveServer::isEnabled();
}

/*static*/ std::shared_ptr<QnSpeechSynthesisDataProvider::OpaqueBackend>
    QnSpeechSynthesisDataProvider::backendInstance(const QString& applicationDirPath)
{
    if (!isEnabled())
        return nullptr;

    // Create the only instance of TextToWaveServer singleton.
    auto textToWaveServer =
        std::make_shared<nx::speech_synthesizer::TextToWaveServer>(applicationDirPath);
    textToWaveServer->start();
    textToWaveServer->waitForStarted();

    return textToWaveServer;
}

QnSpeechSynthesisDataProvider::QnSpeechSynthesisDataProvider(const QString& text):
    QnAbstractStreamDataProvider(QnResourcePtr(new QnResource())),
    m_text(text),
    m_curPos(0)
{
    NX_ASSERT(isEnabled());
}

QnSpeechSynthesisDataProvider::~QnSpeechSynthesisDataProvider()
{
    stop();
}

QnConstMediaContextPtr QnSpeechSynthesisDataProvider::initializeAudioContext(
    const QnAudioFormat& format)
{
    auto codecId = QnFfmpegHelper::fromQtAudioFormatToFfmpegPcmCodec(format);
    if (codecId == AV_CODEC_ID_NONE)
        return QnConstMediaContextPtr();

    auto mediaCtx = std::make_shared<QnAvCodecMediaContext>(codecId);
    auto ctx = mediaCtx->getAvCodecContext();
    if (!ctx)
        return QnConstMediaContextPtr();

    ctx->sample_fmt = QnFfmpegHelper::fromQtAudioFormatToFfmpegSampleType(format);
    if (ctx->sample_fmt == AV_SAMPLE_FMT_NONE)
        return QnConstMediaContextPtr();

    ctx->sample_rate = format.sampleRate();
    ctx->frame_size = kDefaultDataChunkSize / 2;
    ctx->channels = format.channelCount();

    return mediaCtx;
}

QnAbstractMediaDataPtr QnSpeechSynthesisDataProvider::getNextData()
{
    if (m_curPos >= m_rawBuffer.size())
        return QnAbstractMediaDataPtr();

    QnWritableCompressedAudioDataPtr packet(
        new QnWritableCompressedAudioData(kDefaultDataChunkSize, m_ctx));

    const int bytesRest = m_rawBuffer.size() - m_curPos;
    packet->m_data.write(
        m_rawBuffer.constData() + m_curPos,
        (bytesRest < kDefaultDataChunkSize) ? bytesRest : kDefaultDataChunkSize);
    packet->compressionType = AVCodecID::AV_CODEC_ID_PCM_S16LE;
    packet->dataType = QnAbstractMediaData::DataType::AUDIO;
    packet->dataProvider = this;
    m_curPos += kDefaultDataChunkSize;

    return QnAbstractMediaDataPtr(packet);
}

void QnSpeechSynthesisDataProvider::run()
{
    bool status = true;
    m_rawBuffer = doSynthesis(m_text, &status);

    if (!status)
        return;

    while (const auto data = getNextData())
    {
        if (dataCanBeAccepted())
            putData(data);
    }

    afterRun();
}

void QnSpeechSynthesisDataProvider::afterRun()
{
    const QnAbstractMediaDataPtr endOfStream(new QnEmptyMediaData());
    endOfStream->dataProvider = this;
    putData(endOfStream);
}

void QnSpeechSynthesisDataProvider::setText(const QString &text)
{
    m_text = text;
}

QByteArray QnSpeechSynthesisDataProvider::doSynthesis(const QString &text, bool* outStatus)
{
    QBuffer soundBuf;
    QnAudioFormat waveFormat;
    soundBuf.open(QIODevice::WriteOnly);

    *outStatus = true;

    auto ttvInstance = nx::speech_synthesizer::TextToWaveServer::instance();
    ttvInstance->generateSoundSync(text, &soundBuf, &waveFormat);

    m_ctx = initializeAudioContext(waveFormat);
    if (!m_ctx)
        *outStatus = false;

    const std::size_t kSynthesizerOutputHeaderSize = 52;
    return soundBuf.data().mid(kSynthesizerOutputHeaderSize);
}
