#pragma once

#include <atomic>
#include <functional>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <utils/common/long_runnable.h>
#include <nx/utils/singleton.h>
#include <utils/common/systemerror.h>

#include "socket_common.h"

namespace nx {
namespace network {

/*!
    \note Thread-safe
*/
class NX_NETWORK_API DnsResolver
:
    public QnLongRunnable
{
public:
    typedef void* RequestId;
    typedef std::vector<HostAddress> HostAddresses;
    typedef utils::MoveOnlyFunc<void(SystemError::ErrorCode, HostAddresses)> Handler;

    DnsResolver();
    virtual ~DnsResolver();

    virtual void pleaseStop() override;

    /*!
        \param handler MUST not block
        \param requestId Used to cancel request. Multiple requests can be started using same request id
        \return false if failed to start asynchronous resolve operation
        \note It is garanteed that \a reqID is set before \a completionHandler is called
    */
    void resolveAsync(const QString& hostName, Handler handler, int ipVersion, RequestId requestId);
    HostAddresses resolveSync(const QString& hostName, int ipVersion);

    /*!
        \param waitForRunningHandlerCompletion if \a true, this method blocks until running completion handler (if any) has returned
    */
    void cancel(RequestId requestId, bool waitForRunningHandlerCompletion);

    //!Returns \a true if at least one resolve operation is scheduled with \a reqID
    bool isRequestIdKnown(RequestId requestId) const;

    //!Even more priority than /etc/hosts
    void addEtcHost(const QString& name, HostAddresses addresses);
    void removeEtcHost(const QString& name);
    HostAddresses getEtcHost(const QString& name, int ipVersion = 0);

protected:
    //!Implementation of QnLongRunnable::run
    virtual void run();

private:
    class ResolveTask
    {
    public:
        QString hostAddress;
        Handler completionHandler;
        RequestId requestId;
        size_t sequence;
        int ipVersion;

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

    mutable QnMutex m_ectHostsMutex;
    std::map<QString, HostAddresses> m_etcHosts;
};

} // namespace network
} // namespace nx
