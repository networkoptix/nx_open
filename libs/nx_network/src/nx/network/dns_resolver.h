// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/singleton.h>
#include <nx/utils/system_error.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/thread/wait_condition.h>

#include "resolve/abstract_resolver.h"
#include "socket_common.h"

namespace nx {
namespace network {

class PredefinedHostResolver;

static constexpr auto kDefaultDnsResolveTimeout = std::chrono::seconds(15);

class NX_NETWORK_API DnsResolver
{
public:
    using RequestId = void*;
    using Handler = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::deque<HostAddress>)>;

    static constexpr int kDefaultResolveThreadCount = 4;

    DnsResolver(int resolveThreadCount = kDefaultResolveThreadCount);
    virtual ~DnsResolver();

    void stop();

    std::chrono::milliseconds resolveTimeout() const;
    void setResolveTimeout(std::chrono::milliseconds value);

    // TODO: #akolesnikov Use internal sequence instead of RequestId.

    /**
     * @param handler MUST not block
     * @param requestId Used to cancel request. Multiple requests can be started using same request id.
     * @return false if failed to start asynchronous resolve operation
     * NOTE: It is guaranteed that reqID is set before completionHandler is called.
     */
    void resolveAsync(const std::string& hostname, Handler handler, int ipVersion, RequestId requestId);

    SystemError::ErrorCode resolveSync(
        const std::string& hostname,
        int ipVersion,
        ResolveResult* resolveResult);

    // TODO: #akolesnikov following two methods do not belong here.
    /** Has even greater priority than /etc/hosts. */
    void addEtcHost(const std::string& name, std::vector<HostAddress> addresses);
    void removeEtcHost(const std::string& name);

    /**
     * Every attempt to resolve hostname will fails with SystemError::hostNotFound.
     */
    void blockHost(const std::string& hostname);
    void unblockHost(const std::string& hostname);

    /**
     * @param priority Greater value increases priority.
     */
    void registerResolver(std::unique_ptr<AbstractResolver> resolver, int priority);
    int minRegisteredResolverPriority() const;
    int maxRegisteredResolverPriority() const;

protected:
    void resolveThreadMain();
    void reportCachedResultThreadMain();

private:
    class ResolveTask
    {
    public:
        std::string hostAddress;
        Handler completionHandler;
        RequestId requestId = nullptr;
        size_t sequence = 0;
        int ipVersion = AF_INET;
        std::chrono::steady_clock::time_point creationTime;

        ResolveTask(
            std::string hostAddress, Handler handler, RequestId requestId,
            size_t sequence, int ipVersion);
    };

    bool m_terminated = false;
    mutable nx::Mutex m_mutex;
    mutable nx::WaitCondition m_cond;
    std::deque<size_t /*sequence*/> m_taskQueue;
    std::unordered_map<size_t /*sequence*/, ResolveTask> m_tasks;
    std::vector<std::thread> m_resolveThreads;
    std::unordered_set<RequestId> m_runningTaskRequestIds;
    size_t m_currentSequence = 0;
    std::chrono::milliseconds m_resolveTimeout = kDefaultDnsResolveTimeout;
    PredefinedHostResolver* m_predefinedHostResolver = nullptr;
    std::multimap<int, std::unique_ptr<AbstractResolver>, std::greater<int>> m_resolversByPriority;
    std::set<std::string> m_blockedHosts;

    nx::utils::SyncQueue<std::tuple<Handler, std::deque<HostAddress>>> m_cachedResults;
    std::thread m_reportCachedResultThread;

    nx::utils::TimeOutCache<
        std::tuple<int /*ipVersion*/, std::string /*hostname*/>,
        std::deque<HostAddress>,
        std::map
    > m_resolveCache;

    bool isExpired(const ResolveTask& task) const;
};

} // namespace network
} // namespace nx
