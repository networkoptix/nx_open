#include "io_state_fetcher.h"

#include <chrono>

#include <utils/common/synctime.h>

#include <nx/vms/server/nvr/hanwha/common.h>

namespace nx::vms::server::nvr::hanwha {

static const std::chrono::milliseconds kSleepBetweenIterations(1000); //< TODO: change!

static std::set<QnIOStateData> makeFakeData()
{
    // TODO: #dmsihin this is fake implementation! Don't forget to replace with the proper one.
    std::set<QnIOStateData> result;
    for (int i = 0; i < kInputCount; ++i)
    {
        QnIOStateData inputState;
        inputState.id = kInputIdPrefix + QString::number(i);
        inputState.isActive = false;
        inputState.timestamp = qnSyncTime->currentMSecsSinceEpoch();

        result.insert(std::move(inputState));
    }

    for (int i = 0; i < kOutputCount; ++i)
    {
        QnIOStateData outputState;
        outputState.id = kOutputIdPrefix + QString::number(i);
        outputState.isActive = false;
        outputState.timestamp = qnSyncTime->currentMSecsSinceEpoch();

        result.insert(std::move(outputState));
    }

    return result;
}

IoStateFetcher::IoStateFetcher(std::function<void(const std::set<QnIOStateData>&)> stateHandler):
    m_stateHandler(std::move(stateHandler))
{
}

IoStateFetcher::~IoStateFetcher()
{
    stop();
}

void IoStateFetcher::run()
{
    NX_ASSERT(m_stateHandler);

    while (!needToStop())
    {
        std::this_thread::sleep_for(kSleepBetweenIterations);
        m_stateHandler(makeFakeData());
    }
}

} // namespace nx::vms::server::nvr::hanwha
