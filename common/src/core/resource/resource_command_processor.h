#ifndef QN_RESOURCE_COMMAND_PROCESSOR
#define QN_RESOURCE_COMMAND_PROCESSOR

#include "core/dataconsumer/abstract_data_consumer.h"
#include "core/datapacket/abstract_data_packet.h"

#include "resource.h"
#include "resource_consumer.h"

class QnResourceCommand;
typedef QSharedPointer<QnResourceCommand> QnResourceCommandPtr;

class QN_EXPORT QnResourceCommand : public QnAbstractDataPacket, public QnResourceConsumer
{
public:
    QnResourceCommand(QnResourcePtr res);
    virtual ~QnResourceCommand();
    virtual bool execute() = 0;
    virtual void beforeDisconnectFromResource();
};


class QN_EXPORT QnResourceCommandProcessor : public QnAbstractDataConsumer
{
    Q_OBJECT;

public:
    QnResourceCommandProcessor();
    ~QnResourceCommandProcessor();

    virtual void putData(const QnAbstractDataPacketPtr& data);

protected:
    virtual bool processData(QnAbstractDataPacketPtr data);

};

#endif //QN_RESOURCE_COMMAND_PROCESSOR
