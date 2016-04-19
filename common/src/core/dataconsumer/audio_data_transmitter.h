#ifndef AUDIO_DATA_TRANSMITTER_H
#define AUDIO_DATA_TRANSMITTER_H

#include "core/dataconsumer/abstract_data_consumer.h"

class QnAbstractAudioTransmitter : public QnAbstractDataConsumer
{
    Q_OBJECT
public:
    QnAbstractAudioTransmitter();

    virtual bool processData(const QnAbstractDataPacketPtr &data) override;
    virtual bool processAudioData(QnConstAbstractMediaDataPtr &data) = 0;
    virtual bool canAcceptData() const override;
    virtual void putData( const QnAbstractDataPacketPtr& data );
};

#endif // AUDIO_DATA_TRANSMITTER_H
