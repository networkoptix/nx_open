#include "text_to_wav.h"
#include "speach_synthesis_data_provider.h"
#include "../../common/src/core/dataconsumer/audio_data_transmitter.h"
#include "../../common/src/core/datapacket/audio_data_packet.h"
#include "../../common/src/core/resource/resource.h"
#include <QtCore/QBuffer>

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

QnMediaContextPtr QnSpeachSynthesisDataProvider::initializeAudioContext()
{
    auto codec = avcodec_find_decoder(CODEC_ID_PCM_S16LE);

    if (!codec)
        return QnMediaContextPtr();

    auto ctx = avcodec_alloc_context3(codec);

    if (!ctx)
        return QnMediaContextPtr();

    ctx->codec = codec;
    ctx->codec_id = CODEC_ID_PCM_S16LE;
    ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    ctx->sample_rate = 32000;
    ctx->frame_size = kDefaultDataChunkSize / 2;
    ctx->channels = 1;

    return QnMediaContextPtr(new QnMediaContext(ctx));
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

    packet->m_data.write(m_rawBuffer.mid(m_curPos, kDefaultDataChunkSize));
    packet->compressionType = CodecID::CODEC_ID_PCM_S16LE;
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


