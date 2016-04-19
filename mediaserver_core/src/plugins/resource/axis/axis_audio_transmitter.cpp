#include "axis_audio_transmitter.h"
#include <utils/common/sleep.h>

QnAxisAudioTransmitter::QnAxisAudioTransmitter() :
    QnAbstractAudioTransmitter()
{
    qDebug() << "CREATING AXIS DATA TRANSMITTER";
}


bool QnAxisAudioTransmitter::processAudioData(QnConstAbstractMediaDataPtr &data)
{
    qDebug() << "Processsing audio data:" << data->dataSize();

    return true;
}
