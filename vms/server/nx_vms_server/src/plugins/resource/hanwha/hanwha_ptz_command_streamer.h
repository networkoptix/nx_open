#pragma once

#include <map>

#include <nx/network/aio/timer.h>

#include <plugins/resource/hanwha/hanwha_ptz_executor.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaPtzCommandStreamer
{
public:
    HanwhaPtzCommandStreamer(
        const HanwhaResourcePtr& resource,
        const std::map<QString, HanwhaRange>& ranges);

    ~HanwhaPtzCommandStreamer();

    bool continuousMove(const nx::core::ptz::Vector& speedVector);

    bool continuousFocus(qreal speed);

public:
    struct CommandQueueContext
    {
        CommandQueueContext() = default;
        CommandQueueContext(
            const HanwhaResourcePtr& resource,
            const std::map<QString, HanwhaRange>& ranges);

        bool isRunning = false;
        bool doesRequireRepeat = true;
        nx::core::ptz::Vector lastCommand;
        nx::core::ptz::Vector pendingCommand;
        std::unique_ptr<nx::network::aio::Timer> timer;
        std::unique_ptr<HanwhaPtzExecutor> executor;
    };

private:
    bool launchQueue(const HanwhaConfigurationalPtzCommand& command);

    void scheduleNextRequest(
        HanwhaConfigurationalPtzCommandType commandType,
        bool hasRequestSucceeded);

private:
    QnMutex m_mutex;
    std::map<HanwhaConfigurationalPtzCommandType, CommandQueueContext> m_commandQueues;
    bool m_terminated = false;
    int64_t m_sequenceId = 0;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
