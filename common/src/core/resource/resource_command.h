#ifndef QN_RESOURCE_COMMAND_H
#define QN_RESOURCE_COMMAND_H

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/abstract_data_packet.h>
#include <core/resource/resource_consumer.h>

class QnResourceCommand : public QnAbstractDataPacket, public QnResourceConsumer
{
public:
    QnResourceCommand(const QnResourcePtr& res);
    virtual ~QnResourceCommand();
    virtual bool execute() = 0;
    virtual void beforeDisconnectFromResource();
};

#endif // ENABLE_DATA_PROVIDERS

#endif // QN_RESOURCE_COMMAND_H
