#pragma once

#include <nx_ec/ec_api.h>

namespace ec2 {

//!Connection to old (python-based) EC. Does not provide any functionality, every method returns \a notImplemented error
class OldEcConnection: public AbstractECConnection
{
public:
    OldEcConnection(const QnConnectionInfo& connectionInfo);

    virtual QnConnectionInfo connectionInfo() const override;
    virtual void updateConnectionUrl(const nx::utils::Url& url) override;

    virtual AbstractResourceManagerPtr getResourceManager(const Qn::UserAccessData &) override;
    virtual AbstractMediaServerManagerPtr getMediaServerManager(const Qn::UserAccessData &) override;
    virtual AbstractCameraManagerPtr getCameraManager(const Qn::UserAccessData &) override;
    virtual AbstractLicenseManagerPtr getLicenseManager(const Qn::UserAccessData &) override;
    virtual AbstractEventRulesManagerPtr getEventRulesManager(const Qn::UserAccessData &) override;
    virtual AbstractUserManagerPtr getUserManager(const Qn::UserAccessData &) override;
    virtual AbstractLayoutManagerPtr getLayoutManager(const Qn::UserAccessData &) override;
    virtual AbstractLayoutTourManagerPtr getLayoutTourManager(const Qn::UserAccessData& userAccessData) override;
    virtual AbstractVideowallManagerPtr getVideowallManager(const Qn::UserAccessData &) override;
    virtual AbstractWebPageManagerPtr getWebPageManager(const Qn::UserAccessData &) override;
    virtual AbstractStoredFileManagerPtr getStoredFileManager(const Qn::UserAccessData &) override;
    virtual AbstractMiscManagerPtr getMiscManager(const Qn::UserAccessData &) override;
    virtual AbstractDiscoveryManagerPtr getDiscoveryManager(const Qn::UserAccessData &) override;
    virtual AbstractAnalyticsManagerPtr getAnalyticsManager(const Qn::UserAccessData&) override;

    virtual AbstractLicenseNotificationManagerPtr licenseNotificationManager() override;
    virtual AbstractTimeNotificationManagerPtr timeNotificationManager() override;
    virtual AbstractResourceNotificationManagerPtr resourceNotificationManager() override;
    virtual AbstractMediaServerNotificationManagerPtr mediaServerNotificationManager() override;
    virtual AbstractCameraNotificationManagerPtr cameraNotificationManager() override;
    virtual AbstractBusinessEventNotificationManagerPtr businessEventNotificationManager() override;
    virtual AbstractUserNotificationManagerPtr userNotificationManager() override;
    virtual AbstractLayoutNotificationManagerPtr layoutNotificationManager() override;
    virtual AbstractLayoutTourNotificationManagerPtr layoutTourNotificationManager() override;
    virtual AbstractWebPageNotificationManagerPtr webPageNotificationManager() override;
    virtual AbstractDiscoveryNotificationManagerPtr discoveryNotificationManager() override;
    virtual AbstractMiscNotificationManagerPtr miscNotificationManager() override;
    virtual AbstractStoredFileNotificationManagerPtr storedFileNotificationManager() override;
    virtual AbstractVideowallNotificationManagerPtr videowallNotificationManager() override;
    virtual AbstractAnalyticsNotificationManagerPtr analyticsNotificationManager() override;

    virtual void addRemotePeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& _url) override;
    virtual void deleteRemotePeer(const QnUuid& id) override;

    virtual nx::vms::api::Timestamp getTransactionLogTime() const override;
    virtual void setTransactionLogTime(nx::vms::api::Timestamp value) override;

    virtual void startReceivingNotifications() override;
    virtual void stopReceivingNotifications() override;
    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer,
        int* distance,
        nx::network::SocketAddress* knownPeerAddress) const override;
    virtual TransactionMessageBusAdapter* messageBus() const override { return nullptr; }
    virtual nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager() const override { return nullptr; }
    virtual QnCommonModule* commonModule() const override { return nullptr; }
protected:
    virtual int dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler) override;
    virtual int dumpDatabaseToFileAsync(
        const QString& dumpFilePath,
        ec2::impl::SimpleHandlerPtr) override;
    virtual int restoreDatabaseAsync(
        const nx::vms::api::DatabaseDumpData& dbFile,
        impl::SimpleHandlerPtr handler) override;
private:
    QnConnectionInfo m_connectionInfo;
};

} // namespace ec2
