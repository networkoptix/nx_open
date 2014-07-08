#include "resource_command.h"

#ifdef ENABLE_DATA_PROVIDERS

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

#endif // ENABLE_DATA_PROVIDERS