#ifdef ENABLE_DATA_PROVIDERS

#include "audio_data_transmitter.h"
#include <utils/common/sleep.h>
#include <QFile>

QnAbstractAudioTransmitter::QnAbstractAudioTransmitter() :
    QnAbstractDataConsumer(1000),
    m_dataProviderInfo(QnDataProviderInfo(QnAbstractStreamDataProviderPtr())),
    m_transmittedPacketCount(0),
    m_transmittedPacketDuration(0)
{

}

void QnAbstractAudioTransmitter::run()
{
    initSystemThreadId();
    m_elapsedTimer.restart();
    m_transmittedPacketCount = 0;
    m_transmittedPacketDuration = 0;
    while(!needToStop())
    {
        pauseDelay();

        QnAbstractDataPacketPtr data;
        bool get = m_dataQueue.pop(data, 200);

        if (!get)
        {
            QnSleep::msleep(10);
            continue;
        }
        while(!needToStop())
        {
            if (processData(data))
                break;
            else
                QnSleep::msleep(10);
        }

        QnConstAbstractMediaDataPtr media =
            std::dynamic_pointer_cast<const QnAbstractMediaData>(data);

        if(media->dataType == QnAbstractMediaData::AUDIO)
        {
            m_transmittedPacketCount++;
            m_transmittedPacketDuration +=
                std::dynamic_pointer_cast<QnCompressedAudioData>(data)->getDuration();
            makeRealTimeDelay();
        }
    }

    endOfRun();
}

void QnAbstractAudioTransmitter::makeRealTimeDelay()
{
    long diff = m_transmittedPacketDuration - m_elapsedTimer.elapsed();

    if(diff > 0)
        QnSleep::msleep(diff);
}

bool QnAbstractAudioTransmitter::processData(const QnAbstractDataPacketPtr &data)
{
    QnConstAbstractDataPacketPtr constData(data);
    QnConstAbstractMediaDataPtr media = std::dynamic_pointer_cast<const QnAbstractMediaData>(constData);

    auto dataProviderCopy = m_dataProviderInfo.provider; //< this shared pointer could be modified from the other thread
    if (dataProviderCopy && dataProviderCopy.data() == data->dataProvider)
    {
        if (media->dataType == QnAbstractMediaData::AUDIO)
            return processAudioData(media);
    }

    if (media->dataType == QnAbstractMediaData::EMPTY_DATA)
        unsubscribe(dataProviderCopy);

    return true;
}

void QnAbstractAudioTransmitter::unsubscribe(QnAbstractStreamDataProviderPtr dataProvider)
{
    QnMutexLocker lock(&m_mutex);

    QnDataProviderInfo providerInfo(dataProvider);

    dequeueProvider(providerInfo);
    if (dataProvider && dataProvider == m_dataProviderInfo.provider)
    {
        auto owner = dataProvider->getOwner();
        if (owner)
            owner->notInUse(this);

        dataProvider->removeDataProcessor(this);

        if(!m_waitingProviders.isEmpty())
        {
            prepare();
            m_needStop = true;
            m_dataProviderInfo = m_waitingProviders.front();
            m_waitingProviders.pop_front();
            m_dataProviderInfo.provider->addDataProcessor(this);
            if(owner = m_dataProviderInfo.provider->getOwner())
                owner->inUse(this);
            m_dataProviderInfo.provider->startIfNotRunning();
            m_needStop = false;
            start();
        }
        else
        {
            m_dataProviderInfo.provider.reset();
            m_needStop = true;
        }
    }
}

void QnAbstractAudioTransmitter::subscribe(
    QnAbstractStreamDataProviderPtr dataProvider,
    int priority)
{
    if (!dataProvider)
        return;

    QnMutexLocker lock(&m_mutex);

    QnDataProviderInfo providerInfo(dataProvider, priority);
    if (m_dataProviderInfo.provider)
    {
        if(m_dataProviderInfo == providerInfo)
        {
            return;
        }
        else if(m_dataProviderInfo < providerInfo)
        {
            m_needStop = true;
            if (auto owner = m_dataProviderInfo.provider->getOwner())
                owner->notInUse(this);
            m_dataProviderInfo.provider->removeDataProcessor(this);
            enqueueProvider(m_dataProviderInfo);

        }
        else
        {
            enqueueProvider(providerInfo);
            return;
        }
    }

    m_dataQueue.clear();

    prepare();
    if (auto owner = providerInfo.provider->getOwner())
        owner->inUse(this);

    m_dataProviderInfo = providerInfo;
    providerInfo.provider->addDataProcessor(this);
    providerInfo.provider->startIfNotRunning();
    m_needStop = false;

    start();
}

void QnAbstractAudioTransmitter::enqueueProvider(const QnDataProviderInfo &info)
{
    dequeueProvider(info);
    auto it = m_waitingProviders.begin();
    auto end = m_waitingProviders.end();

    for(; it != end; it++)
    {
        if(*it < info)
        {
            m_waitingProviders.insert(it, info);
            break;
        }
    }

    if(it == end)
        m_waitingProviders.push_back(info);
}

void QnAbstractAudioTransmitter::dequeueProvider(const QnDataProviderInfo &info)
{
    m_waitingProviders.removeAll(info);
}

#endif // ENABLE_DATA_PROVIDERS
