#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/abstract_data_packet.h>
#include <core/resource/resource_consumer.h>

class QnResourceCommand: public QnAbstractDataPacket, public QnResourceConsumer
{
public:
    QnResourceCommand(const QnResourcePtr& res);
    virtual ~QnResourceCommand();
    virtual bool execute() = 0;
};

#endif // ENABLE_DATA_PROVIDERS
