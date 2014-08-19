/**********************************************************
* 23 jul 2014
* a.kolesnikov
***********************************************************/

#include "old_ec_connection.h"

#include <utils/common/concurrent.h>
#include <utils/common/scoped_thread_rollback.h>

#include "ec2_thread_pool.h"
#include "transaction/transaction.h"


namespace ec2
{
    OldEcConnection::OldEcConnection(const QnConnectionInfo& connectionInfo)
    :
        m_connectionInfo(connectionInfo)
    {
    }

    QnConnectionInfo OldEcConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    AbstractResourceManagerPtr OldEcConnection::getResourceManager()
    {
        return AbstractResourceManagerPtr();
    }

    AbstractMediaServerManagerPtr OldEcConnection::getMediaServerManager()
    {
        return AbstractMediaServerManagerPtr();
    }

    AbstractCameraManagerPtr OldEcConnection::getCameraManager()
    {
        return AbstractCameraManagerPtr();
    }

    AbstractLicenseManagerPtr OldEcConnection::getLicenseManager()
    {
        return AbstractLicenseManagerPtr();
    }

    AbstractBusinessEventManagerPtr OldEcConnection::getBusinessEventManager()
    {
        return AbstractBusinessEventManagerPtr();
    }

    AbstractUserManagerPtr OldEcConnection::getUserManager()
    {
        return AbstractUserManagerPtr();
    }

    AbstractLayoutManagerPtr OldEcConnection::getLayoutManager()
    {
        return AbstractLayoutManagerPtr();
    }

    AbstractVideowallManagerPtr OldEcConnection::getVideowallManager()
    {
        return AbstractVideowallManagerPtr();
    }

    AbstractStoredFileManagerPtr OldEcConnection::getStoredFileManager()
    {
        return AbstractStoredFileManagerPtr();
    }

    AbstractUpdatesManagerPtr OldEcConnection::getUpdatesManager()
    {
        return AbstractUpdatesManagerPtr();
    }

    AbstractMiscManagerPtr OldEcConnection::getMiscManager()
    {
        return AbstractMiscManagerPtr();
    }

    AbstractDiscoveryManagerPtr OldEcConnection::getDiscoveryManager()
    {
        return AbstractDiscoveryManagerPtr();
    }

    int OldEcConnection::setPanicMode(Qn::PanicMode /*value*/, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnScopedThreadRollback ensureFreeThread(1, Ec2ThreadPool::instance());
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

    int OldEcConnection::getCurrentTime(impl::CurrentTimeHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnScopedThreadRollback ensureFreeThread(1, Ec2ThreadPool::instance());
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::CurrentTimeHandler::done, handler, reqID, ec2::ErrorCode::notImplemented, 0));
        return reqID;
    }

    int OldEcConnection::dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnScopedThreadRollback ensureFreeThread(1, Ec2ThreadPool::instance());
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::DumpDatabaseHandler::done, handler, reqID, ec2::ErrorCode::notImplemented, ec2::ApiDatabaseDumpData()));
        return reqID;
    }

    int OldEcConnection::restoreDatabaseAsync(const ApiDatabaseDumpData& /*dbFile*/, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnScopedThreadRollback ensureFreeThread(1, Ec2ThreadPool::instance());
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

    int OldEcConnection::forcePrimaryTimeServer( const QUuid& /*serverGuid*/, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
        QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( &impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented ) );
        return reqID;
    }

    void OldEcConnection::addRemotePeer(const QUrl& /*url*/, const QUuid& /*peerGuid*/)
    {
    }

    void OldEcConnection::deleteRemotePeer(const QUrl& /*url*/)
    {
    }

    void OldEcConnection::sendRuntimeData(const ec2::ApiRuntimeData& /*data*/)
    {
    }

    void OldEcConnection::startReceivingNotifications()
    {
    }
}
