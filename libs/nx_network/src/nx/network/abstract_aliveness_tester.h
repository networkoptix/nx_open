#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>

#include "socket_common.h"

namespace nx::network {

/**
 * Implements keep-alive logic.
 * In most cases it is checking aliveness of a connection or a remote peer.
 */
class NX_NETWORK_API AbstractAlivenessTester:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using ProbeResultHandler =
        nx::utils::MoveOnlyFunc<void(bool /*probeReturned*/)>;

    AbstractAlivenessTester(const KeepAliveOptions& keepAliveOptions);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * @param onTestFailure Invoked if aliveness test failed. After this every pending probe
     * is considered failed and its completion is not expected and ignored.
     * AbstractAlivenessTester::start can be called again after completion.
     */
    void start(nx::utils::MoveOnlyFunc<void()> onTestFailure);

    /**
     * Cancels all operations.
     * AbstractAlivenessTester::start can be called again after this method returns.
     */
    void cancelSync();

    /**
     * This method may be called if user is aware about aliveness.
     * This call may postpone the next AbstractAlivenessTester::probe call if it makes sense.
     */
    void confirmAliveness();

protected:
    virtual void probe(ProbeResultHandler handler) = 0;

    /**
     * Cancels any scheduled probe immediately and without blocking.
     * NOTE: Always called from object's AIO thread.
     */
    virtual void cancelProbe() = 0;

    virtual void stopWhileInAioThread() override;

private:
    const KeepAliveOptions m_keepAliveOptions;
    aio::Timer m_timer;
    nx::utils::MoveOnlyFunc<void()> m_onTestFailure;
    int m_probeNumber = 0;

    void resetState();

    void launchTimer();

    void handleTimerEvent();

    void reportFailure();

    void handleProbeResult(bool success);
};

} // namespace nx::network
