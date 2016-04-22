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

    if(media->dataType == QnAbstractMediaData::AUDIO)
    {
        return processAudioData(media);
    }

    return true;
}

QSet<CodecID> QnAbstractAudioTransmitter::getSupportedAudioCodecs()
{
    return QSet<CodecID>();
}
