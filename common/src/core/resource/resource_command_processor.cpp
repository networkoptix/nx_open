#include "resource_command_processor.h"
#include "resource.h"

QnResourceCommand::QnResourceCommand(QnResourcePtr res):
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

void QnResourceCommandProcessor::putData(QnAbstractDataPacketPtr data)
{
    QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();
    QnAbstractDataConsumer::putData(data);
}


bool QnResourceCommandProcessor::processData(QnAbstractDataPacketPtr data)
{
	QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();
    command->execute();
    return true;
}

