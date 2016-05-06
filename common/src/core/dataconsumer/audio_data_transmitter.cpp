#include "audio_data_transmitter.h"
#include <utils/common/sleep.h>
#include <QFile>

QnAbstractAudioTransmitter::QnAbstractAudioTransmitter() :
    QnAbstractDataConsumer(1000)
{
}

bool QnAbstractAudioTransmitter::processData(const QnAbstractDataPacketPtr &data)
{
    QnConstAbstractDataPacketPtr constData(data);
    QnConstAbstractMediaDataPtr media = std::dynamic_pointer_cast<const QnAbstractMediaData>(constData);

    auto dataProviderCopy = m_dataProvider; //< this shared pointer could be modified from the other thread
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
    if (dataProvider && dataProvider == m_dataProvider)
    {
        auto owner = dataProvider->getOwner();
        if (owner)
            owner->notInUse(this);
        dataProvider->removeDataProcessor(this);
        m_dataProvider.clear();
        m_needStop = true;
    }
}

void QnAbstractAudioTransmitter::subscribe(QnAbstractStreamDataProviderPtr dataProvider)
{
    QnMutexLocker lock(&m_mutex);
    prepare();
    m_needStop = false;

    if (m_dataProvider)
    {
        if (auto owner = m_dataProvider->getOwner())
            owner->notInUse(this);
        m_dataProvider->removeDataProcessor(this);
    }

    m_dataProvider = dataProvider;
    dataProvider->addDataProcessor(this);
    dataProvider->startIfNotRunning();
    if (auto owner = dataProvider->getOwner())
        owner->inUse(this);
    start();
}
