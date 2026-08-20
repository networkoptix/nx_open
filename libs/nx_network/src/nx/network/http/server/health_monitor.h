// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include <nx/network/aio/timer.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

namespace nx::network::http::server {

/**
 * The liveness and the readiness state of a service, refreshed in the background.
 *
 * Both checks run periodically here and their results are cached, so that a health request is
 * answered from memory. A check that never completes is reported as failed once its cached
 * result gets older than the staleness limit.
 *
 * Liveness is a self-request to the application HTTP listener: any response proves its accept
 * loop, its parser and its request pipeline end to end, an error status included (an
 * unregistered path is expected to answer 403 or 404 - it is the response that matters, not its
 * code). Only a transport-level failure counts as dead. Until the first check completes,
 * liveness is reported as alive.
 *
 * Readiness is the set of probes registered by the service; a dependency that has not been
 * checked yet is reported as not ready. With no probes registered the service is always ready,
 * matching the Go nxhttp package.
 */
class NX_NETWORK_API HealthMonitor
{
public:
    /** Reports the probe outcome: std::nullopt if the dependency is healthy, an error if not. */
    using ProbeHandler = nx::MoveOnlyFunc<void(std::optional<std::string> /*error*/)>;

    /**
     * Checks a single readiness dependency (SQL reachable, a key cache loaded, ...). Invoked in
     * an aio thread, so it must not block; the result may be reported from any thread, at any
     * later time - including never, and including after this object is gone.
     */
    using Probe = std::function<void(ProbeHandler)>;

    /** The name reported for a failing liveness check. */
    static inline const std::string kLivenessCheckName = "listener";

    /**
     * The path the liveness self-request is sent to. It is not expected to be registered on the
     * application listener: its rejection response proves the listener just as well.
     */
    static inline const std::string kLivenessSelfRequestPath = "/livez";

    static constexpr std::chrono::milliseconds kDefaultCheckPeriod = std::chrono::seconds(5);

    /** The shortest period accepted. */
    static constexpr std::chrono::milliseconds kMinCheckPeriod = std::chrono::milliseconds(200);

    /** A result older than this many check periods is reported as a failure. */
    static constexpr int kStalePeriodCount = 3;

    struct Status
    {
        bool ok = true;

        /** Check name -> failure reason. Empty while ok. */
        std::map<std::string, std::string> failed;
    };

    HealthMonitor() = default;
    ~HealthMonitor();

    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;

    /**
     * Adds a named dependency check to run on every cycle. Registering the same name again
     * replaces the previous probe.
     * NOTE: Must be called before start().
     */
    void registerReadinessProbe(std::string name, Probe probe);

    /**
     * The application listener the liveness self-request is sent to.
     * NOTE: Required, and must be called before start(): without a target there is nothing for
     * liveness to report but the process being up,.
     */
    void setLivenessTarget(const SocketAddress& endpoint, bool isSecure);

    /**
     * The period between check cycles. Every check is given half of it to complete, so that a
     * cycle can refresh each result; a check outliving its cycle is skipped by the next one and
     * eventually reported as stale.
     * NOTE: `period` must be at least kMinCheckPeriod.
     * NOTE: Must be called before start().
     */
    void setCheckPeriod(std::chrono::milliseconds period);

    /**
     * Runs the first cycle immediately, then one every check period. Starting twice is a no-op.
     */
    void start();

    /**
     * Stops the cycle. Checks already in flight may still report their result afterwards, which
     * is safe: their handler holds the state rather than this object.
     * NOTE: Must be called from the thread owning this object - never from an aio thread (where
     * the synchronous stop of the timer can deadlock), and never from a check callback.
     */
    void stop();

    Status liveness() const;
    Status readiness() const;

private:
    struct CheckResult
    {
        std::optional<std::string> error;
        std::chrono::steady_clock::time_point completedAt;
    };

    struct State
    {
        mutable nx::Mutex mutex;
        std::optional<CheckResult> liveness;
        bool livenessCheckInFlight = false;
        std::map<std::string, CheckResult> readiness;
        std::set<std::string> readinessChecksInFlight;
    };

    /**
     * The handler a probe is invoked with. Reports the result when the probe calls it, and, if
     * the probe destroys it without calling - which the Probe contract allows - forgets the check
     * so that the next cycle makes it again instead of counting it as running forever.
     */
    class ProbeCall
    {
    public:
        ProbeCall(std::shared_ptr<State> state, std::string name);
        ~ProbeCall();

        ProbeCall(ProbeCall&&) = default;
        ProbeCall& operator=(ProbeCall&&) = default;

        void operator()(std::optional<std::string> error);

    private:
        std::shared_ptr<State> m_state;
        std::string m_name;
    };

    void runChecks();
    void checkLiveness();
    void checkReadiness();

    bool isStale(const CheckResult& result) const;
    void addFailure(Status* status, const std::string& name, const std::string& error) const;

private:
    // Kept behind a shared_ptr so that a check reporting its result after this object is gone
    // writes into a state that is still alive rather than into freed memory.
    const std::shared_ptr<State> m_state = std::make_shared<State>();
    std::chrono::milliseconds m_checkPeriod = kDefaultCheckPeriod;
    std::map<std::string, Probe> m_probes;
    nx::Url m_livenessUrl;
    bool m_isStarted = false;
    aio::Timer m_timer;
    std::unique_ptr<AsyncClient> m_livenessClient;
};

} // namespace nx::network::http::server
