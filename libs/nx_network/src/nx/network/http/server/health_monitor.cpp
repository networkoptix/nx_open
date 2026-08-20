// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "health_monitor.h"

#include <cstring>
#include <utility>

#include <nx/network/socket_factory.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::network::http::server {

namespace {

constexpr char kStaleError[] = "the check result is stale";
constexpr char kNotCheckedError[] = "not checked yet";

/** Whether the address is the one a listener binds to accept on every interface. */
bool isWildcard(const HostAddress& address)
{
    if (address.isEmpty())
        return true;

    if (const auto ipV4 = address.ipV4())
        return ipV4->s_addr == INADDR_ANY;

    const auto ipV6 = address.ipV6().first;
    return ipV6 && memcmp(&*ipV6, &in6addr_any, sizeof(in6_addr)) == 0;
}

} // namespace

HealthMonitor::~HealthMonitor()
{
    stop();
}

void HealthMonitor::registerReadinessProbe(std::string name, Probe probe)
{
    if (!NX_ASSERT(!m_isStarted, "Readiness probe %1 is registered after the start", name))
        return;

    if (m_probes.contains(name))
        NX_WARNING(this, "Readiness probe %1 is re-registered", name);

    m_probes[std::move(name)] = std::move(probe);
}

void HealthMonitor::setLivenessTarget(const SocketAddress& endpoint, bool isSecure)
{
    if (!NX_ASSERT(!m_isStarted, "The liveness target is set after the start"))
        return;

    // A wildcard listen address is not connectable; the loopback interface of the same port
    // reaches the same listener.
    auto target = endpoint;
    if (isWildcard(target.address))
    {
        target.address = HostAddress::localhost.toPureIpAddress(
            target.address.isPureIpV6() ? AF_INET6 : AF_INET);
    }

    m_livenessUrl = url::Builder()
        .setScheme(isSecure ? kSecureUrlSchemeName : kUrlSchemeName)
        .setEndpoint(target)
        .setPath(kLivenessSelfRequestPath);
}

void HealthMonitor::setCheckPeriod(std::chrono::milliseconds period)
{
    if (!NX_ASSERT(!m_isStarted, "The check period is set after the start"))
        return;

    if (!NX_ASSERT(period >= kMinCheckPeriod,
        "The check period %1 is shorter than the %2 minimum", period, kMinCheckPeriod))
    {
        return;
    }

    m_checkPeriod = period;
}

void HealthMonitor::start()
{
    // Starting twice would run two cycles at once, doubling the rate every check is made at.
    if (m_isStarted)
        return;

    m_isStarted = true;

    // Without one, /livez would answer ok whatever happened to the service, which is worse than
    // not serving it at all.
    if (!NX_ASSERT(!m_livenessUrl.isEmpty(), "No liveness target is set"))
        return;

    m_timer.post([this]() { runChecks(); });
}

void HealthMonitor::stop()
{
    NX_ASSERT(!m_timer.isInSelfAioThread(),
        "stop() must not be called from the thread running the checks");

    // Stopping the timer first: after it, no cycle is running, so m_livenessClient is not being
    // replaced by the aio thread anymore.
    m_timer.pleaseStopSync();
    if (m_livenessClient)
        m_livenessClient->pleaseStopSync();
}

HealthMonitor::Status HealthMonitor::liveness() const
{
    if (m_livenessUrl.isEmpty())
        return {};

    NX_MUTEX_LOCKER lock(&m_state->mutex);

    // Never report a process as dead before its first check has had the chance to complete: a
    // restart is the response to a liveness failure.
    if (!m_state->liveness)
        return {};

    Status status;
    if (m_state->liveness->error)
        addFailure(&status, kLivenessCheckName, *m_state->liveness->error);
    else if (isStale(*m_state->liveness))
        addFailure(&status, kLivenessCheckName, kStaleError);

    return status;
}

HealthMonitor::Status HealthMonitor::readiness() const
{
    Status status;

    NX_MUTEX_LOCKER lock(&m_state->mutex);
    for (const auto& [name, probe]: m_probes)
    {
        const auto it = m_state->readiness.find(name);
        if (it == m_state->readiness.end())
            addFailure(&status, name, kNotCheckedError);
        else if (it->second.error)
            addFailure(&status, name, *it->second.error);
        else if (isStale(it->second))
            addFailure(&status, name, kStaleError);
    }

    return status;
}

void HealthMonitor::runChecks()
{
    checkLiveness();
    checkReadiness();

    m_timer.start(m_checkPeriod, [this]() { runChecks(); });
}

void HealthMonitor::checkLiveness()
{
    if (m_livenessUrl.isEmpty())
        return;

    {
        NX_MUTEX_LOCKER lock(&m_state->mutex);
        if (m_state->livenessCheckInFlight)
            return;
        m_state->livenessCheckInFlight = true;
    }

    // A new client for every check: it must prove a fresh accept rather than that a pooled
    // connection is still open. Replacing it here, in the aio thread it is bound to and outside
    // of its own handler, is what makes destroying it safe.
    m_livenessClient = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
    m_livenessClient->bindToAioThread(m_timer.getAioThread());
    m_livenessClient->setMaxNumberOfRedirects(0);

    // Half a period, so that the request completes within the cycle that issued it and the next
    // cycle finds a fresh result. kMinCheckPeriod is what keeps this a real timeout - zero would
    // mean no timeout at all for the client.
    m_livenessClient->setAllTimeouts(m_checkPeriod / 2);

    m_livenessClient->doGet(m_livenessUrl,
        [this]()
        {
            // The client is stopped before this object is destroyed, so `this` is alive here.
            std::optional<std::string> error;
            if (m_livenessClient->failed())
            {
                error = "the application listener did not answer";

                if (const auto systemError = m_livenessClient->lastSysErrorCode();
                    systemError != SystemError::noError)
                {
                    *error += ": " + SystemError::toString(systemError);
                }
            }

            NX_MUTEX_LOCKER lock(&m_state->mutex);
            m_state->liveness = CheckResult{
                .error = std::move(error),
                .completedAt = std::chrono::steady_clock::now()};
            m_state->livenessCheckInFlight = false;
        });
}

void HealthMonitor::checkReadiness()
{
    for (const auto& [name, probe]: m_probes)
    {
        {
            NX_MUTEX_LOCKER lock(&m_state->mutex);
            // A probe of an earlier cycle that still holds its handler is not asked again: its
            // result going stale is what reports the dependency as stuck.
            if (!m_state->readinessChecksInFlight.insert(name).second)
                continue;
        }

        // Invoked with no lock held: a probe may report its result, or drop the handler, right
        // here in this thread.
        probe(ProbeCall(m_state, name));
    }
}

HealthMonitor::ProbeCall::ProbeCall(std::shared_ptr<State> state, std::string name):
    m_state(std::move(state)),
    m_name(std::move(name))
{
}

HealthMonitor::ProbeCall::~ProbeCall()
{
    if (!m_state)
        return; //< The result has been reported, or this call has been moved from.

    // Destroyed with nothing reported.
    NX_MUTEX_LOCKER lock(&m_state->mutex);
    m_state->readinessChecksInFlight.erase(m_name);
}

void HealthMonitor::ProbeCall::operator()(std::optional<std::string> error)
{
    const auto state = std::exchange(m_state, nullptr);
    if (!state)
        return;

    NX_MUTEX_LOCKER lock(&state->mutex);
    state->readiness[m_name] = CheckResult{
        .error = std::move(error),
        .completedAt = std::chrono::steady_clock::now()};
    state->readinessChecksInFlight.erase(m_name);
}

bool HealthMonitor::isStale(const CheckResult& result) const
{
    return std::chrono::steady_clock::now() - result.completedAt
        > m_checkPeriod * kStalePeriodCount;
}

void HealthMonitor::addFailure(
    Status* status,
    const std::string& name,
    const std::string& error) const
{
    status->ok = false;
    status->failed[name] = error;
}

} // namespace nx::network::http::server
