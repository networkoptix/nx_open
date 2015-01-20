/**********************************************************
* 20 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef HOST_ADDRESS_RESOLVER_H
#define HOST_ADDRESS_RESOLVER_H

#include <atomic>
#include <functional>

#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

#include <utils/common/long_runnable.h>
#include <utils/common/singleton.h>
#include <utils/common/systemerror.h>

#include "socket_common.h"


/*!
    \note Thread-safe
*/
class HostAddressResolver
:
    public QnLongRunnable,
    public Singleton<HostAddressResolver>
{
public:
    typedef void* RequestID;

    HostAddressResolver();
    virtual ~HostAddressResolver();

    //!Implementation of QnLongRunnable::pleaseStop
    virtual void pleaseStop() override;

    /*!
        \param reqID Used to cancel request. Multiple requests can be started using same request id
        \return false if failed to start asynchronous resolve operation
        \note It is garanteed that \a reqID is set before \a completionHandler is called
    */
    bool resolveAddressAsync(
        const HostAddress& addressToResolve,
        std::function<void (SystemError::ErrorCode, const HostAddress&)>&& completionHandler,
        RequestID reqID );
    /*!
        \note This method is re-enterable
    */
    bool resolveAddressSync( const QString& hostName, HostAddress* const resolvedAddress );
    //!Returns \a true if address \a addr is resolved
    bool isAddressResolved( const HostAddress& addr ) const;
    /*!
        \param waitForRunningHandlerCompletion if \a true, this method blocks until running completion handler (if any) has returned
    */
    void cancel( RequestID reqID, bool waitForRunningHandlerCompletion );
    //!Returns \a true if at least one resolve operation is scheduled with \a reqID
    bool isRequestIDKnown( RequestID reqID ) const;

protected:
    //!Implementation of QnLongRunnable::run
    virtual void run();

private:
    class ResolveTask
    {
    public:
        HostAddress hostAddress;
        std::function<void(SystemError::ErrorCode, const HostAddress&)> completionHandler;
        RequestID reqID;

        ResolveTask(
            HostAddress _hostAddress,
            std::function<void(SystemError::ErrorCode, const HostAddress&)> _completionHandler,
            RequestID _reqID );
    };

    bool m_terminated;
    mutable QMutex m_mutex;
    mutable QWaitCondition m_cond;
    std::list<ResolveTask> m_taskQueue;
    RequestID m_runningTaskReqID;
};

#endif  //HOST_ADDRESS_RESOLVER_H
