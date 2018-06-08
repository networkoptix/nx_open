#include "hanwha_ptz_command_streamer.h"

namespace nx {
namespace mediaserver_core {
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
        {HanwhaConfigurationalPtzCommandType::focus, false},
        {HanwhaConfigurationalPtzCommandType::zoom, false},
        {HanwhaConfigurationalPtzCommandType::ptr, true}
    };

    for (auto entry: kQueueTypes)
    {
        const auto queueType = entry.first;
        const auto isAbsolute = entry.second;

        CommandQueueContext context(resource, ranges);
        context.isAbsolute = isAbsolute;
        context.executor->setCommandDoneCallback(
            [this](HanwhaConfigurationalPtzCommandType commandType)
            {
                scheduleNextRequest(commandType);
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
            nx::core::ptz::Vector(speed, 0.0, 0.0, 0.0)
        });
}

bool HanwhaPtzCommandStreamer::launchQueue(const HanwhaConfigurationalPtzCommand& command)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return false;

    auto& queue = m_commandQueues[command.command];
    queue.pendingCommand = command.speed;

    bool result = true;
    if (!queue.isRunning && !queue.pendingCommand.isNull())
        result = queue.executor->executeCommand(command);

    queue.isRunning = result;
    if (!result)
        queue.pendingCommand = nx::core::ptz::Vector();

    return result;
}

void HanwhaPtzCommandStreamer::scheduleNextRequest(HanwhaConfigurationalPtzCommandType commandType)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    auto contextItr = m_commandQueues.find(commandType);
    if (contextItr == m_commandQueues.cend())
    {
        NX_ASSERT(false, lm("Can't find command queue %1").arg((int) commandType));
        return;
    }

    auto& context = contextItr->second;
    if (context.pendingCommand.isNull() || context.isAbsolute)
    {
        context.isRunning = false;
        return;
    }
    else
    {
        context.timer->start(
            kExecutionInterval,
            [this, &context, commandType]()
            {
                QnMutexLocker lock(&m_mutex);
                if (m_terminated)
                    return;

                context.executor->executeCommand({commandType, context.pendingCommand});
            });
    }
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
