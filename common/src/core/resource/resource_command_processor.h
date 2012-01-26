#ifndef QN_RESOURCE_COMMAND_PROCESSOR
#define QN_RESOURCE_COMMAND_PROCESSOR

#include "core/dataconsumer/dataconsumer.h"
#include "core/datapacket/datapacket.h"

#include "resource.h"
#include "resource_consumer.h"

class QnResourceCommand;
typedef QSharedPointer<QnResourceCommand> QnResourceCommandPtr;

class QN_EXPORT QnResourceCommand : public QnAbstractDataPacket, public QnResourceConsumer
{
public:
    QnResourceCommand(QnResourcePtr res);
    virtual ~QnResourceCommand();
    virtual void execute() = 0;
    virtual void beforeDisconnectFromResource();
};


class QN_EXPORT QnResourceCommandProcessor : public QnAbstractDataConsumer
{
    Q_OBJECT;

public:
    QnResourceCommandProcessor();
    ~QnResourceCommandProcessor();

    virtual void putData(QnAbstractDataPacketPtr data);

    virtual void clearUnprocessedData();

    bool hasSuchResourceInQueue(QnResourcePtr res) const;

protected:
    virtual bool processData(QnAbstractDataPacketPtr data);

private:
    mutable QMutex m_cs;
    QMap<QnId, unsigned int> mResourceQueue;
};

#endif //QN_RESOURCE_COMMAND_PROCESSOR
