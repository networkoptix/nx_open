#ifndef AXIS_AUDIO_TRANSMITTER_H
#define AXIS_AUDIO_TRANSMITTER_H

#include "core/dataconsumer/audio_data_transmitter.h"

class QnAxisAudioTransmitter : public QnAbstractAudioTransmitter
{
    Q_OBJECT
public:
    QnAxisAudioTransmitter();

    virtual bool processAudioData(QnConstAbstractMediaDataPtr &data) override;

};

#endif // AXIS_AUDIO_TRANSMITTER_H
