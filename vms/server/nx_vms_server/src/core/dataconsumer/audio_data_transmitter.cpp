#include "audio_data_transmitter.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include <set>

#include <utils/common/sleep.h>
#include <core/resource/resource.h>

AVCodecID QnAbstractAudioTransmitter::toFfmpegCodec(const QString& codec, int* outDefaultBitrate)
{
    int bitratePos = codec.lastIndexOf(L'-');
    if (bitratePos >= 0)
    {
        *outDefaultBitrate = codec.mid(bitratePos + 1).toInt();
        if (*outDefaultBitrate < 1000)
            *outDefaultBitrate *= 1000; // Convert KHz to Hz.
    }

    const auto lCodec = codec.toLower();
    if (lCodec == lit("aac") || lCodec == lit("mpeg4-generic"))
        return AV_CODEC_ID_AAC;
    else if (lCodec.startsWith(lit("g726")))
        return AV_CODEC_ID_ADPCM_G726;
    else if (lCodec == lit("pcmu") || lCodec == lit("mulaw"))
        return AV_CODEC_ID_PCM_MULAW;
    else if (lCodec == lit("pcma") || lCodec == lit("alaw"))
        return AV_CODEC_ID_PCM_ALAW;
    else if (lCodec == lit("pcm_s16Le"))
        return AV_CODEC_ID_PCM_S16LE;
    else
        return AV_CODEC_ID_NONE;
}


QnAbstractAudioTransmitter::QnAbstractAudioTransmitter():
    QnAbstractDataConsumer(1000),
    m_transmittedPacketDuration(0),
    m_prevUsedProvider(nullptr)
{
}

void QnAbstractAudioTransmitter::endOfRun()
{
    QnMutexLocker lock(&m_mutex);
    while (!m_providers.empty())
        unsubscribeUnsafe(m_providers.begin()->second.data());
    m_prevUsedProvider = nullptr;
}

void QnAbstractAudioTransmitter::makeRealTimeDelay(const QnConstCompressedAudioDataPtr& audioData)
{
    m_transmittedPacketDuration += audioData->getDurationMs();
    qint64 diff = m_transmittedPacketDuration - m_elapsedTimer.elapsed();
    if(diff > 0)
        QnSleep::msleep(diff);
}

bool QnAbstractAudioTransmitter::processData(const QnAbstractDataPacketPtr& data)
{
    QnConstAbstractMediaDataPtr media = std::dynamic_pointer_cast<const QnAbstractMediaData>(data);
    if (!media)
        return true;

    if (media->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        unsubscribe(media->dataProvider);
        return true;
    }

    {
        QnMutexLocker lock(&m_mutex);
        if (m_providers.empty() || m_providers.begin()->second.data() != media->dataProvider)
            return true; //< Data from non-active provider.
    }

    if (auto audioData = std::dynamic_pointer_cast<const QnCompressedAudioData>(data))
    {
        if (m_prevUsedProvider != audioData->dataProvider)
        {
            m_prevUsedProvider = audioData->dataProvider;
            m_transmittedPacketDuration = 0;
            m_elapsedTimer.restart();
            prepare();
        }
        if (!processAudioData(audioData))
            return true; //< Skip audio data anyway. false means keep data to process it further again.

        makeRealTimeDelay(audioData);
    }

    return true;
}

void QnAbstractAudioTransmitter::unsubscribe(QnAbstractStreamDataProvider* dataProvider)
{
    QnMutexLocker lock(&m_mutex);
    unsubscribeUnsafe(dataProvider);
}

void QnAbstractAudioTransmitter::removePacketsByProvider(
    QnAbstractStreamDataProvider* dataProvider)
{
    bool removeSince = false;
    std::set<QnAbstractStreamDataProvider*> providersToFlush;

    for (auto it = m_providers.begin(); it != m_providers.end(); ++it)
    {
        if (it->second.data() == dataProvider)
            removeSince = true;

        if (!removeSince)
            continue;

        providersToFlush.insert(it->second.data());
    }

    auto randomAccess = m_dataQueue.lock();
    for (int i = randomAccess.size() - 1; i >= 0; --i)
    {
        auto packet = std::dynamic_pointer_cast<const QnAbstractMediaData>(randomAccess.at(i));
        if (packet && providersToFlush.count(packet->dataProvider))
            randomAccess.removeAt(i);
    }
}

void QnAbstractAudioTransmitter::unsubscribeUnsafe(QnAbstractStreamDataProvider* dataProvider)
{
    for (auto itr = m_providers.begin(); itr != m_providers.end(); ++itr)
    {
        auto provider = itr->second;
        if (provider == dataProvider)
        {
            if (auto owner = provider->getOwner())
                owner->notInUse(this);
            provider->removeDataProcessor(this);
            removePacketsByProvider(dataProvider);
            m_providers.erase(itr);
            break;
        }
    }

    if (m_providers.empty())
        pleaseStop();
}

void QnAbstractAudioTransmitter::subscribe(
    QnAbstractStreamDataProviderPtr dataProvider,
    int priorityClass)
{
    if (!dataProvider)
        return;

    // Add a timestamp to the priority class (6 lower bytes are the timestamp, 2 higher bytes are
    // the priorityClass).
    qint64 priority = ((qint64) priorityClass << 48) + QDateTime::currentMSecsSinceEpoch();

    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_providers.begin(); itr != m_providers.end(); ++itr)
    {
        if (itr->second == dataProvider)
            return; //< Already exists.
    }

    m_providers.emplace(priority, dataProvider);

    dataProvider->addDataProcessor(this);
    if (auto owner = dataProvider->getOwner())
        owner->inUse(this);
    dataProvider->startIfNotRunning();

    start();
}

#endif // defined(ENABLE_DATA_PROVIDERS)
