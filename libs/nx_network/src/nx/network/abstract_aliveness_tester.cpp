#include "abstract_aliveness_tester.h"

namespace nx::network {

AbstractAlivenessTester::AbstractAlivenessTester(
    const KeepAliveOptions& keepAliveOptions)
    :
    m_keepAliveOptions(keepAliveOptions)
{
    NX_ASSERT(
        m_keepAliveOptions.inactivityPeriodBeforeFirstProbe > std::chrono::seconds::zero() &&
        (m_keepAliveOptions.probeCount == 0 ||
            m_keepAliveOptions.probeSendPeriod > std::chrono::seconds::zero()));

    bindToAioThread(getAioThread());
}

void AbstractAlivenessTester::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
}

void AbstractAlivenessTester::start(
    nx::utils::MoveOnlyFunc<void()> onTestFailure)
{
    dispatch(
        [this, onTestFailure = std::move(onTestFailure)]() mutable
        {
            m_onTestFailure = std::move(onTestFailure);

            resetState();
            launchTimer();
        });
}

void AbstractAlivenessTester::cancelSync()
{
    executeInAioThreadSync(
        [this]()
        {
            m_timer.cancelSync();
            cancelProbe();
        });
}

void AbstractAlivenessTester::confirmAliveness()
{
    dispatch(
        [this]()
        {
            resetState();
            launchTimer();
        });
}

void AbstractAlivenessTester::stopWhileInAioThread()
{
    cancelProbe();

    base_type::stopWhileInAioThread();

    m_timer.pleaseStopSync();
}

void AbstractAlivenessTester::resetState()
{
    m_timer.cancelSync();
    m_probeNumber = 0;
}

void AbstractAlivenessTester::launchTimer()
{
    m_timer.start(
        m_probeNumber == 0
            ? m_keepAliveOptions.inactivityPeriodBeforeFirstProbe
            : m_keepAliveOptions.probeSendPeriod,
        [this]() { handleTimerEvent(); });
}

void AbstractAlivenessTester::handleTimerEvent()
{
    if (m_probeNumber == m_keepAliveOptions.probeCount)
    {
        cancelProbe();
        resetState();
        return reportFailure();
    }

    ++m_probeNumber;

    probe([this](auto&&... args) {
        handleProbeResult(std::forward<decltype(args)>(args)...); });

    launchTimer();
}

void AbstractAlivenessTester::reportFailure()
{
    nx::utils::swapAndCall(m_onTestFailure);
}

void AbstractAlivenessTester::handleProbeResult(bool success)
{
    if (success)
    {
        resetState();
        launchTimer();
    }
}

} // namespace nx::network
