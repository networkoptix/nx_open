#include "resource_command_processor.h"
#include "resource.h"

bool sameResourceFunctor(const QnAbstractDataPacketPtr& data, QVariant resource)
{
    QnResourcePtr res = resource.value<QnResourcePtr>();
    QnResourceCommand* command = dynamic_cast<QnResourceCommand*>(data.data());
    if (!command)
        return false;

    if (command->getResource()->getUniqueId() == res->getUniqueId())
        return true;

    return false;
}



QnResourceCommand::QnResourceCommand(const QnResourcePtr& res):
QnResourceConsumer(res)
{
    
}

QnResourceCommand::~QnResourceCommand()
{
    disconnectFromResource();
};

void QnResourceCommand::beforeDisconnectFromResource()
{
    disconnectFromResource();
}

/////////////////////


QnResourceCommandProcessor::QnResourceCommandProcessor():
    QnAbstractDataConsumer(1000)
{
    //start(); This is static singleton and run() uses log (another singleton). Jusst in case I will start it with first device created.
}

QnResourceCommandProcessor::~QnResourceCommandProcessor()
{
    stop();
}

void QnResourceCommandProcessor::putData(const QnAbstractDataPacketPtr& data)
{
    QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();
    QnAbstractDataConsumer::putData(data);
}


bool QnResourceCommandProcessor::processData(const QnAbstractDataPacketPtr& data)
{
    if (!data)
        return true;
    QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();

    bool result = command->execute();

    if (!result) // if command failed for this resource. => resource must be not available, lets remove everything related to the resouce from the queue
        m_dataQueue.detachDataByCondition(sameResourceFunctor, QVariant::fromValue<QnResourcePtr>(command->getResource()) );
        
    

    return true;
}

