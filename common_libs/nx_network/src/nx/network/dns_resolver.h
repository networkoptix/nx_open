#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/system_error.h>

#include "resolve/predefined_host_resolver.h"
#include "resolve/system_resolver.h"
#include "socket_common.h"

namespace nx {
namespace network {

class NX_NETWORK_API DnsResolver:
    public QnLongRunnable
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
    void resolveAsync(const QString& hostName, Handler handler, int ipVersion, RequestId requestId);
    SystemError::ErrorCode resolveSync(
        const QString& hostName,
        int ipVersion,
        std::deque<HostAddress>* resolvedAddresses);

    /**
     * @param waitForRunningHandlerCompletion if true, this method blocks until
     * running completion handler (if any) returns.
     */
    void cancel(RequestId requestId, bool waitForRunningHandlerCompletion);

    bool isRequestIdKnown(RequestId requestId) const;

    // TODO: #ak following two methods do not belong here.
    /** Has even greater priority than /etc/hosts. */
    void addEtcHost(const QString& name, std::vector<HostAddress> addresses);
    void removeEtcHost(const QString& name);

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
        QString hostAddress;
        Handler completionHandler;
        RequestId requestId;
        size_t sequence;
        int ipVersion;
        std::chrono::steady_clock::time_point creationTime;

        ResolveTask(
            QString hostAddress, Handler handler, RequestId requestId,
            size_t sequence, int ipVersion);
    };

    bool m_terminated;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
    std::list<ResolveTask> m_taskQueue;
    RequestId m_runningTaskRequestId;
    size_t m_currentSequence;
    std::chrono::milliseconds m_resolveTimeout;
    PredefinedHostResolver* m_predefinedHostResolver;
    std::multimap<int, std::unique_ptr<AbstractResolver>, std::greater<int>> m_resolversByPriority;

    bool isExpired(const ResolveTask& task) const;
};

} // namespace network
} // namespace nx
