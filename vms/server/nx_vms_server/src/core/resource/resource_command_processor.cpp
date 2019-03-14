#include "resource_command_processor.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <core/resource/resource.h>

#include "resource_command.h"

bool sameResourceFunctor(const QnAbstractDataPacketPtr& data, const QVariant& resource)
{
    const auto res = resource.value<QnResourcePtr>();
    const auto command = dynamic_cast<QnResourceCommand*>(data.get());
    return command && command->getResource() == res;
}

QnResourceCommandProcessor::QnResourceCommandProcessor():
    QnAbstractDataConsumer(1000)
{
}

QnResourceCommandProcessor::~QnResourceCommandProcessor()
{
    stop();
}

bool QnResourceCommandProcessor::hasCommandsForResource(const QnResourcePtr& resource) const
{
    auto queue = m_dataQueue.lock();
    for (int i = 0; i < queue.size(); ++i)
    {
        const auto command = dynamic_cast<QnResourceCommand*>(queue.at(i).get());
        if (command && command->getResource() == resource)
            return true;
    }
    return false;
}

bool QnResourceCommandProcessor::processData(const QnAbstractDataPacketPtr& data)
{
    if (!data)
        return true;
    QnResourceCommandPtr command = std::static_pointer_cast<QnResourceCommand>(data);

    bool result = command->execute();

    // If command failed for this resource => resource must be not available, lets remove
    // everything related to the resouce from the queue.
    if (!result)
    {
        m_dataQueue.detachDataByCondition(
            sameResourceFunctor,
            QVariant::fromValue<QnResourcePtr>(command->getResource()));
    }

    return true;
}

#endif // ENABLE_DATA_PROVIDERS

