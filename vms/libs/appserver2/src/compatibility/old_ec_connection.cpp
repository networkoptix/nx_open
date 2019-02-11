/**********************************************************
* 23 jul 2014
* a.kolesnikov
***********************************************************/

#include "old_ec_connection.h"

#include <nx/utils/concurrent.h>
#include <utils/common/scoped_thread_rollback.h>

#include "ec2_thread_pool.h"
#include "transaction/transaction.h"

#include <nx/vms/api/data/database_dump_data.h>

namespace ec2 {

OldEcConnection::OldEcConnection(const QnConnectionInfo& connectionInfo):
    m_connectionInfo(connectionInfo)
{
}

QnConnectionInfo OldEcConnection::connectionInfo() const
{
    return m_connectionInfo;
}

void OldEcConnection::updateConnectionUrl(const nx::utils::Url & /*url*/)
{
    NX_ASSERT(false, "Should never get here");
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

AbstractEventRulesManagerPtr OldEcConnection::getEventRulesManager(const Qn::UserAccessData &)
{
    return {};
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

AbstractAnalyticsManagerPtr OldEcConnection::getAnalyticsManager(const Qn::UserAccessData&)
{
    return AbstractAnalyticsManagerPtr();
}

    AbstractLicenseNotificationManagerPtr OldEcConnection::licenseNotificationManager()
{
    return AbstractLicenseNotificationManagerPtr();
}

AbstractTimeNotificationManagerPtr OldEcConnection::timeNotificationManager()
{
    return AbstractTimeNotificationManagerPtr();
}

AbstractResourceNotificationManagerPtr OldEcConnection::resourceNotificationManager()
{
    return AbstractResourceNotificationManagerPtr();
}

AbstractMediaServerNotificationManagerPtr OldEcConnection::mediaServerNotificationManager()
{
    return AbstractMediaServerNotificationManagerPtr();
}

AbstractCameraNotificationManagerPtr OldEcConnection::cameraNotificationManager()
{
    return AbstractCameraNotificationManagerPtr();
}

AbstractBusinessEventNotificationManagerPtr OldEcConnection::businessEventNotificationManager()
{
    return AbstractBusinessEventNotificationManagerPtr();
}

AbstractUserNotificationManagerPtr OldEcConnection::userNotificationManager()
{
    return AbstractUserNotificationManagerPtr();
}

AbstractLayoutNotificationManagerPtr OldEcConnection::layoutNotificationManager()
{
    return AbstractLayoutNotificationManagerPtr();
}

AbstractLayoutTourNotificationManagerPtr OldEcConnection::layoutTourNotificationManager()
{
    return AbstractLayoutTourNotificationManagerPtr();
}

AbstractWebPageNotificationManagerPtr OldEcConnection::webPageNotificationManager()
{
    return AbstractWebPageNotificationManagerPtr();
}

AbstractDiscoveryNotificationManagerPtr OldEcConnection::discoveryNotificationManager()
{
    return AbstractDiscoveryNotificationManagerPtr();
}

AbstractMiscNotificationManagerPtr OldEcConnection::miscNotificationManager()
{
    return AbstractMiscNotificationManagerPtr();
}

AbstractUpdatesNotificationManagerPtr OldEcConnection::updatesNotificationManager()
{
    return AbstractUpdatesNotificationManagerPtr();
}

AbstractStoredFileNotificationManagerPtr OldEcConnection::storedFileNotificationManager()
{
    return AbstractStoredFileNotificationManagerPtr();
}

AbstractVideowallNotificationManagerPtr OldEcConnection::videowallNotificationManager()
{
    return AbstractVideowallNotificationManagerPtr();
}

AbstractAnalyticsNotificationManagerPtr OldEcConnection::analyticsNotificationManager()
{
    return AbstractAnalyticsNotificationManagerPtr();
}

int OldEcConnection::dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler)
{
    const int reqID = generateRequestID();
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(
            &impl::DumpDatabaseHandler::done,
            handler,
            reqID,
            ec2::ErrorCode::notImplemented,
            nx::vms::api::DatabaseDumpData()));
    return reqID;
}

    int OldEcConnection::dumpDatabaseToFileAsync( const QString& /*dumpFilePath*/, ec2::impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(), std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
        return reqID;
    }

int OldEcConnection::restoreDatabaseAsync(
    const nx::vms::api::DatabaseDumpData& /*dbFile*/,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(&impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented));
    return reqID;
}

    void OldEcConnection::addRemotePeer(
        const QnUuid& /*id*/,
        nx::vms::api::PeerType /*peerType*/,
        const nx::utils::Url & /*url*/)
    {
    }

    void OldEcConnection::deleteRemotePeer(const QnUuid& /*id*/)
    {
    }

    nx::vms::api::Timestamp OldEcConnection::getTransactionLogTime() const {
        return {};
    }

    void OldEcConnection::setTransactionLogTime(nx::vms::api::Timestamp /*value*/)
    {

    }

    void OldEcConnection::startReceivingNotifications()
    {
    }

    void OldEcConnection::stopReceivingNotifications()
    {
    }

    QnUuid OldEcConnection::routeToPeerVia(const QnUuid& /*uuid*/, int*, nx::network::SocketAddress*) const
    {
        return QnUuid();
    }

}
