#include "hanwha_ptz_command_streamer.h"

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const std::chrono::milliseconds kExecutionInterval(300);

} // namespace

HanwhaPtzCommandStreamer::CommandQueueContext::CommandQueueContext(
    const HanwhaResourcePtr& resource,
    const std::map<QString, HanwhaRange>& ranges)
    :
    timer(std::make_unique<nx::network::aio::Timer>()),
    executor(std::make_unique<HanwhaPtzExecutor>(resource, ranges))
{
}

HanwhaPtzCommandStreamer::HanwhaPtzCommandStreamer(
    const HanwhaResourcePtr& resource,
    const std::map<QString, HanwhaRange>& ranges)
{
    static const std::map<HanwhaConfigurationalPtzCommandType, bool> kQueueTypes = {
        {HanwhaConfigurationalPtzCommandType::focus, true},
        {HanwhaConfigurationalPtzCommandType::zoom, true},
        {HanwhaConfigurationalPtzCommandType::ptr, false}
    };

    for (auto entry: kQueueTypes)
    {
        const auto queueType = entry.first;
        const auto doesRequireRepeat = entry.second;

        CommandQueueContext context(resource, ranges);
        context.doesRequireRepeat = doesRequireRepeat;
        context.executor->setCommandDoneCallback(
            [this](HanwhaConfigurationalPtzCommandType commandType, bool hasRequestSucceeded)
            {
                scheduleNextRequest(commandType, hasRequestSucceeded);
            });

        m_commandQueues.emplace(queueType, std::move(context));
    }
}

HanwhaPtzCommandStreamer::~HanwhaPtzCommandStreamer()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    for (auto& entry: m_commandQueues)
    {
        auto& context = entry.second;
        context.timer->pleaseStopSync();
        context.executor->stop();
    }
}

bool HanwhaPtzCommandStreamer::continuousMove(const nx::core::ptz::Vector& speedVector)
{
    bool result = launchQueue(
        {
            HanwhaConfigurationalPtzCommandType::zoom,
            nx::core::ptz::Vector(0.0, 0.0, 0.0, speedVector.zoom)
        });

    if (!result)
        return result;

    result = launchQueue(
        {
            HanwhaConfigurationalPtzCommandType::ptr,
            nx::core::ptz::Vector(speedVector.pan, speedVector.tilt, speedVector.rotation, 0.0)
        });

    return result;
}

bool HanwhaPtzCommandStreamer::continuousFocus(qreal speed)
{
    return launchQueue(
        {
            HanwhaConfigurationalPtzCommandType::focus,
            nx::core::ptz::Vector(0.0, 0.0, 0.0, 0.0, speed)
        });
}

bool HanwhaPtzCommandStreamer::launchQueue(const HanwhaConfigurationalPtzCommand& command)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return false;

    auto& queue = m_commandQueues[command.command];
    queue.pendingCommand = command.speed;

    const bool needToRunQueue = !queue.isRunning
        && (!queue.pendingCommand.isNull() || !queue.doesRequireRepeat);

    if (needToRunQueue)
    {
        queue.isRunning = queue.executor->executeCommand(command, ++m_sequenceId);
        if (!queue.isRunning)
        {
            queue.pendingCommand = nx::core::ptz::Vector();
            queue.lastCommand = nx::core::ptz::Vector();
            return false;
        }
        queue.lastCommand = command.speed;
    }

    return true;
}

void HanwhaPtzCommandStreamer::scheduleNextRequest(
    HanwhaConfigurationalPtzCommandType commandType,
    bool hasRequestSucceeded)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    auto contextItr = m_commandQueues.find(commandType);
    if (contextItr == m_commandQueues.cend())
    {
        NX_ASSERT(false, lm("Can't find command queue %1").arg((int)commandType));
        return;
    }

    auto& context = contextItr->second;
    if (context.doesRequireRepeat)
    {
        if (context.pendingCommand.isNull())
        {
            context.isRunning = false;
            return;
        }
        else
        {
            context.timer->start(
                hasRequestSucceeded ? kExecutionInterval : std::chrono::milliseconds::zero(),
                [this, &context, commandType]()
                {
                    QnMutexLocker lock(&m_mutex);
                    if (m_terminated)
                        return;

                    context.executor->executeCommand(
                        {commandType, context.pendingCommand},
                        ++m_sequenceId);
                });
        }
    }
    else
    {
        const bool isTheSameCommand = qFuzzyEquals(context.lastCommand, context.pendingCommand);
        if (!isTheSameCommand || !hasRequestSucceeded)
        {
            context.lastCommand = context.pendingCommand;
            context.executor->executeCommand(
                {commandType, context.pendingCommand},
                ++m_sequenceId);
        }
        else
        {
            context.isRunning = false;
        }
    }
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
