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

    QString OldEcConnection::authInfo() const
    {
        return m_connectionInfo.ecUrl.password();
    }

    AbstractResourceManagerPtr OldEcConnection::getResourceManager()
    {
        return AbstractResourceManagerPtr();
    }

    AbstractMediaServerManagerPtr OldEcConnection::getMediaServerManager(const Qn::UserAccessData &)
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

    AbstractUserManagerPtr OldEcConnection::getUserManager(const Qn::UserAccessData &)
    {
        return AbstractUserManagerPtr();
    }

    AbstractLayoutManagerPtr getLayoutManager(const Qn::UserAccessData &)
    {
        return AbstractLayoutManagerPtr();
    }

    AbstractVideowallManagerPtr OldEcConnection::getVideowallManager(const Qn::UserAccessData &)
    {
        return AbstractVideowallManagerPtr();
    }

    AbstractWebPageManagerPtr OldEcConnection::getWebPageManager(const Qn::UserAccessData &)
    {
        return AbstractWebPageManagerPtr();
    }

    AbstractStoredFileManagerPtr OldEcConnection::getStoredFileManager()
    {
        return AbstractStoredFileManagerPtr();
    }

    AbstractUpdatesManagerPtr OldEcConnection::getUpdatesManager(const Qn::UserAccessData &)
    {
        return AbstractUpdatesManagerPtr();
    }

    AbstractMiscManagerPtr OldEcConnection::getMiscManager(const Qn::UserAccessData &)
    {
        return AbstractMiscManagerPtr();
    }

    AbstractDiscoveryManagerPtr OldEcConnection::getDiscoveryManager(const Qn::UserAccessData &)
    {
        return AbstractDiscoveryManagerPtr();
    }

    AbstractTimeManagerPtr OldEcConnection::getTimeManager()
    {
        return AbstractTimeManagerPtr();
    }

    int OldEcConnection::dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::DumpDatabaseHandler::done, handler, reqID, ec2::ErrorCode::notImplemented, ec2::ApiDatabaseDumpData()));
        return reqID;
    }

    int OldEcConnection::dumpDatabaseToFileAsync( const QString& /*dumpFilePath*/, ec2::impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

    int OldEcConnection::restoreDatabaseAsync(const ApiDatabaseDumpData& /*dbFile*/, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

    void OldEcConnection::addRemotePeer(const QUrl& /*url*/)
    {
    }

    void OldEcConnection::deleteRemotePeer(const QUrl& /*url*/)
    {
    }

    void OldEcConnection::sendRuntimeData(const ec2::ApiRuntimeData& /*data*/)
    {
    }

    qint64 OldEcConnection::getTransactionLogTime() const {
        return -1;
    }

    void OldEcConnection::setTransactionLogTime(qint64 /* value */)
    {

    }


    void OldEcConnection::startReceivingNotifications()
    {
    }

    void OldEcConnection::stopReceivingNotifications()
    {
    }

    QnUuid OldEcConnection::routeToPeerVia(const QnUuid& uuid, int* ) const
    {
        Q_UNUSED(uuid);
        return QnUuid();
    }

}
