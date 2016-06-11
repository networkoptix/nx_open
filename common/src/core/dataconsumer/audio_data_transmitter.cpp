#ifdef ENABLE_DATA_PROVIDERS

#include "audio_data_transmitter.h"
#include <utils/common/sleep.h>
#include <core/resource/resource.h>
#include <QFile>

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
    qint64  diff = m_transmittedPacketDuration - m_elapsedTimer.elapsed();
    if(diff > 0)
        QnSleep::msleep(diff);
}

bool QnAbstractAudioTransmitter::processData(const QnAbstractDataPacketPtr &data)
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
        if (m_providers.empty() || m_providers.begin()->second != data->dataProvider)
            return true; //< data from non active provider
    }

    if (auto audioData = std::dynamic_pointer_cast<const QnCompressedAudioData>(data))
    {
        if (m_prevUsedProvider != data->dataProvider)
        {
            m_prevUsedProvider = data->dataProvider;
            m_transmittedPacketDuration = 0;
            m_elapsedTimer.restart();
            prepare();
        }
        if (!processAudioData(audioData))
            return false;

        makeRealTimeDelay(audioData);
    }

    return true;
}

void QnAbstractAudioTransmitter::unsubscribe(QnAbstractStreamDataProvider* dataProvider)
{
    QnMutexLocker lock(&m_mutex);
    unsubscribeUnsafe(dataProvider);
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

    // add timestamp to priority class (6 lower bytes is timestamp, 2 high byte priorityClass)
    qint64 priority = ((qint64)priorityClass << 48) + QDateTime::currentMSecsSinceEpoch();

    QnMutexLocker lock(&m_mutex);

    m_providers.emplace(priority, dataProvider);

    dataProvider->addDataProcessor(this);
    if (auto owner = dataProvider->getOwner())
        owner->inUse(this);
    dataProvider->startIfNotRunning();

    start();
}

#endif // ENABLE_DATA_PROVIDERS
