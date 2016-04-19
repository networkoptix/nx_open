#include "audio_data_transmitter.h"
#include <utils/common/sleep.h>

QnAbstractAudioTransmitter::QnAbstractAudioTransmitter() :
    QnAbstractDataConsumer(1000)
{
    qDebug() << "Abstract Audio data Transmitter ctor";
}

bool QnAbstractAudioTransmitter::processData(const QnAbstractDataPacketPtr &data)
{
    qDebug() << "PRocessiung DATA!";
    QnConstAbstractDataPacketPtr constData(data);
    QnConstAbstractMediaDataPtr media = std::dynamic_pointer_cast<const QnAbstractMediaData>(constData);

    if(media->dataType == QnAbstractMediaData::AUDIO)
        return processAudioData(media);

    return true;
}


bool QnAbstractAudioTransmitter::canAcceptData() const
{
    qDebug() << "CAN ACCEPT DATA - YES";
    return true;
}

void QnAbstractAudioTransmitter::putData( const QnAbstractDataPacketPtr& data )
{
    qDebug() << "PUTTING DATA TO  AUDIO TRANSMITTER";
    if (!needToStop())
    {
        m_dataQueue.push(data);
        qDebug() << "QUEUE SIZE" << m_dataQueue.size();
    }
}
