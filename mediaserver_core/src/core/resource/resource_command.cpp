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

#endif // ENABLE_DATA_PROVIDERS