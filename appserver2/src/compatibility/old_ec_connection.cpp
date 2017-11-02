/**********************************************************
* 23 jul 2014
* a.kolesnikov
***********************************************************/

#include "old_ec_connection.h"

#include <nx/utils/concurrent.h>
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

    void OldEcConnection::updateConnectionUrl(const nx::utils::Url & /*url*/)
    {
        NX_EXPECT(false, "Should never get here");
    }

    AbstractResourceManagerPtr OldEcConnection::getResourceManager(const Qn::UserAccessData &)
    {
        return AbstractResourceManagerPtr();
    }

    AbstractMediaServerManagerPtr OldEcConnection::getMediaServerManager(const Qn::UserAccessData &)
    {
        return AbstractMediaServerManagerPtr();
    }

    AbstractCameraManagerPtr OldEcConnection::getCameraManager(const Qn::UserAccessData &)
    {
        return AbstractCameraManagerPtr();
    }

    AbstractLicenseManagerPtr OldEcConnection::getLicenseManager(const Qn::UserAccessData &)
    {
        return AbstractLicenseManagerPtr();
    }

    AbstractBusinessEventManagerPtr OldEcConnection::getBusinessEventManager(const Qn::UserAccessData &)
    {
        return AbstractBusinessEventManagerPtr();
    }

    AbstractUserManagerPtr OldEcConnection::getUserManager(const Qn::UserAccessData &)
    {
        return AbstractUserManagerPtr();
    }

    AbstractLayoutManagerPtr OldEcConnection::getLayoutManager(const Qn::UserAccessData &)
    {
        return AbstractLayoutManagerPtr();
    }

    AbstractLayoutTourManagerPtr OldEcConnection::getLayoutTourManager(const Qn::UserAccessData&)
    {
        return AbstractLayoutTourManagerPtr();
    }

    AbstractVideowallManagerPtr OldEcConnection::getVideowallManager(const Qn::UserAccessData &)
    {
        return AbstractVideowallManagerPtr();
    }

    AbstractWebPageManagerPtr OldEcConnection::getWebPageManager(const Qn::UserAccessData &)
    {
        return AbstractWebPageManagerPtr();
    }

    AbstractStoredFileManagerPtr OldEcConnection::getStoredFileManager(const Qn::UserAccessData &)
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

    AbstractTimeManagerPtr OldEcConnection::getTimeManager(const Qn::UserAccessData &)
    {
        return AbstractTimeManagerPtr();
    }

    AbstractLicenseNotificationManagerPtr OldEcConnection::getLicenseNotificationManager()
    {
        return AbstractLicenseNotificationManagerPtr();
    }

    AbstractTimeNotificationManagerPtr OldEcConnection::getTimeNotificationManager()
    {
        return AbstractTimeNotificationManagerPtr();
    }

    AbstractResourceNotificationManagerPtr OldEcConnection::getResourceNotificationManager()
    {
        return AbstractResourceNotificationManagerPtr();
    }

    AbstractMediaServerNotificationManagerPtr OldEcConnection::getMediaServerNotificationManager()
    {
        return AbstractMediaServerNotificationManagerPtr();
    }

    AbstractCameraNotificationManagerPtr OldEcConnection::getCameraNotificationManager()
    {
        return AbstractCameraNotificationManagerPtr();
    }

    AbstractBusinessEventNotificationManagerPtr OldEcConnection::getBusinessEventNotificationManager()
    {
        return AbstractBusinessEventNotificationManagerPtr();
    }

    AbstractUserNotificationManagerPtr OldEcConnection::getUserNotificationManager()
    {
        return AbstractUserNotificationManagerPtr();
    }

    AbstractLayoutNotificationManagerPtr OldEcConnection::getLayoutNotificationManager()
    {
        return AbstractLayoutNotificationManagerPtr();
    }

    AbstractLayoutTourNotificationManagerPtr OldEcConnection::getLayoutTourNotificationManager()
    {
        return AbstractLayoutTourNotificationManagerPtr();
    }

    AbstractWebPageNotificationManagerPtr OldEcConnection::getWebPageNotificationManager()
    {
        return AbstractWebPageNotificationManagerPtr();
    }

    AbstractDiscoveryNotificationManagerPtr OldEcConnection::getDiscoveryNotificationManager()
    {
        return AbstractDiscoveryNotificationManagerPtr();
    }

    AbstractMiscNotificationManagerPtr OldEcConnection::getMiscNotificationManager()
    {
        return AbstractMiscNotificationManagerPtr();
    }

    AbstractUpdatesNotificationManagerPtr OldEcConnection::getUpdatesNotificationManager()
    {
        return AbstractUpdatesNotificationManagerPtr();
    }

    AbstractStoredFileNotificationManagerPtr OldEcConnection::getStoredFileNotificationManager()
    {
        return AbstractStoredFileNotificationManagerPtr();
    }

    AbstractVideowallNotificationManagerPtr OldEcConnection::getVideowallNotificationManager()
    {
        return AbstractVideowallNotificationManagerPtr();
    }

    int OldEcConnection::dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::DumpDatabaseHandler::done, handler, reqID, ec2::ErrorCode::notImplemented, ec2::ApiDatabaseDumpData()));
        return reqID;
    }

    int OldEcConnection::dumpDatabaseToFileAsync( const QString& /*dumpFilePath*/, ec2::impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

    int OldEcConnection::restoreDatabaseAsync(const ApiDatabaseDumpData& /*dbFile*/, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

    void OldEcConnection::addRemotePeer(const QnUuid& /*id*/, const nx::utils::Url & /*url*/)
    {
    }

    void OldEcConnection::deleteRemotePeer(const QnUuid& /*id*/)
    {
    }

    Timestamp OldEcConnection::getTransactionLogTime() const {
        return Timestamp();
    }

    void OldEcConnection::setTransactionLogTime(Timestamp /* value */)
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
