#include "audio_data_transmitter.h"
#include <utils/common/sleep.h>
#include <QFile>

QnAbstractAudioTransmitter::QnAbstractAudioTransmitter() :
    QnAbstractDataConsumer(1000)
{
    qDebug() << "Abstract Audio data Transmitter ctor";
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

    return true;
}

void QnAbstractAudioTransmitter::unsubscribe(QnLiveStreamProviderPtr dataProvider)
{
    if (dataProvider && dataProvider == m_dataProvider)
    {
        dataProvider->getOwner()->notInUse(this);
        dataProvider->removeDataProcessor(this);
        m_dataProvider.clear();
        stop();
    }
}


void QnAbstractAudioTransmitter::subscribe(QnLiveStreamProviderPtr dataProvider)
{
    if (m_dataProvider)
    {
        m_dataProvider->getOwner()->notInUse(this);
        m_dataProvider->removeDataProcessor(this);
    }

    m_dataProvider = dataProvider;
    dataProvider->addDataProcessor(this);
    dataProvider->startIfNotRunning();
    dataProvider->getOwner()->inUse(this);
    start();
}
