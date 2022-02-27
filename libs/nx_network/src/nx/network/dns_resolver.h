// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <set>

#include <nx/utils/move_only_func.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/utils/thread/thread.h>
#include <nx/utils/system_error.h>

#include "resolve/abstract_resolver.h"
#include "socket_common.h"

namespace nx {
namespace network {

class PredefinedHostResolver;

static constexpr auto kDefaultDnsResolveTimeout = std::chrono::seconds(15);

class NX_NETWORK_API DnsResolver:
    public nx::utils::Thread
{
public:
    typedef void* RequestId;
    typedef utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::deque<HostAddress>)> Handler;

    DnsResolver();
    virtual ~DnsResolver();

    virtual void pleaseStop() override;

    std::chrono::milliseconds resolveTimeout() const;
    void setResolveTimeout(std::chrono::milliseconds value);

    /**
     * @param handler MUST not block
     * @param requestId Used to cancel request. Multiple requests can be started using same request id.
     * @return false if failed to start asynchronous resolve operation
     * NOTE: It is garanteed that reqID is set before completionHandler is called.
     */
    void resolveAsync(const std::string& hostName, Handler handler, int ipVersion, RequestId requestId);

    SystemError::ErrorCode resolveSync(
        const std::string& hostName,
        int ipVersion,
        std::deque<HostAddress>* resolvedAddresses);

    /**
     * @param waitForRunningHandlerCompletion if true, this method blocks until
     * running completion handler (if any) returns.
     */
    void cancel(RequestId requestId, bool waitForRunningHandlerCompletion);

    bool isRequestIdKnown(RequestId requestId) const;

    // TODO: #akolesnikov following two methods do not belong here.
    /** Has even greater priority than /etc/hosts. */
    void addEtcHost(const std::string& name, std::vector<HostAddress> addresses);
    void removeEtcHost(const std::string& name);

    /**
     * Every attempt to resolve hostName will fails with SystemError::hostNotFound.
     */
    void blockHost(const std::string& hostName);
    void unblockHost(const std::string& hostName);

    /**
     * @param priority Greater value increases priority.
     */
    void registerResolver(std::unique_ptr<AbstractResolver> resolver, int priority);
    int minRegisteredResolverPriority() const;
    int maxRegisteredResolverPriority() const;

protected:
    virtual void run() override;

private:
    class ResolveTask
    {
    public:
        std::string hostAddress;
        Handler completionHandler;
        RequestId requestId;
        size_t sequence;
        int ipVersion;
        std::chrono::steady_clock::time_point creationTime;

        ResolveTask(
            std::string hostAddress, Handler handler, RequestId requestId,
            size_t sequence, int ipVersion);
    };

    bool m_terminated = false;
    mutable nx::Mutex m_mutex;
    mutable nx::WaitCondition m_cond;
    std::list<ResolveTask> m_taskQueue;
    RequestId m_runningTaskRequestId = nullptr;
    size_t m_currentSequence = 0;
    std::chrono::milliseconds m_resolveTimeout = kDefaultDnsResolveTimeout;
    PredefinedHostResolver* m_predefinedHostResolver = nullptr;
    std::multimap<int, std::unique_ptr<AbstractResolver>, std::greater<int>> m_resolversByPriority;
    std::set<std::string> m_blockedHosts;

    bool isExpired(const ResolveTask& task) const;
};

} // namespace network
} // namespace nx
