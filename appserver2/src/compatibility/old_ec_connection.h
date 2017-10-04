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
    virtual AbstractBusinessEventManagerPtr getBusinessEventManager(const Qn::UserAccessData &) override;
    virtual AbstractUserManagerPtr getUserManager(const Qn::UserAccessData &) override;
    virtual AbstractLayoutManagerPtr getLayoutManager(const Qn::UserAccessData &) override;
    virtual AbstractLayoutTourManagerPtr getLayoutTourManager(const Qn::UserAccessData& userAccessData) override;
    virtual AbstractVideowallManagerPtr getVideowallManager(const Qn::UserAccessData &) override;
    virtual AbstractWebPageManagerPtr getWebPageManager(const Qn::UserAccessData &) override;
    virtual AbstractStoredFileManagerPtr getStoredFileManager(const Qn::UserAccessData &) override;
    virtual AbstractUpdatesManagerPtr getUpdatesManager(const Qn::UserAccessData &) override;
    virtual AbstractMiscManagerPtr getMiscManager(const Qn::UserAccessData &) override;
    virtual AbstractDiscoveryManagerPtr getDiscoveryManager(const Qn::UserAccessData &) override;
    virtual AbstractTimeManagerPtr getTimeManager(const Qn::UserAccessData &) override;

    virtual AbstractLicenseNotificationManagerPtr getLicenseNotificationManager() override;
    virtual AbstractTimeNotificationManagerPtr getTimeNotificationManager() override;
    virtual AbstractResourceNotificationManagerPtr getResourceNotificationManager() override;
    virtual AbstractMediaServerNotificationManagerPtr getMediaServerNotificationManager() override;
    virtual AbstractCameraNotificationManagerPtr getCameraNotificationManager() override;
    virtual AbstractBusinessEventNotificationManagerPtr getBusinessEventNotificationManager() override;
    virtual AbstractUserNotificationManagerPtr getUserNotificationManager() override;
    virtual AbstractLayoutNotificationManagerPtr getLayoutNotificationManager() override;
    virtual AbstractLayoutTourNotificationManagerPtr getLayoutTourNotificationManager() override;
    virtual AbstractWebPageNotificationManagerPtr getWebPageNotificationManager() override;
    virtual AbstractDiscoveryNotificationManagerPtr getDiscoveryNotificationManager() override;
    virtual AbstractMiscNotificationManagerPtr getMiscNotificationManager() override;
    virtual AbstractUpdatesNotificationManagerPtr getUpdatesNotificationManager() override;
    virtual AbstractStoredFileNotificationManagerPtr getStoredFileNotificationManager() override;
    virtual AbstractVideowallNotificationManagerPtr getVideowallNotificationManager() override;

    virtual void addRemotePeer(const QnUuid& id, const nx::utils::Url& _url) override;
    virtual void deleteRemotePeer(const QnUuid& id) override;

    virtual Timestamp getTransactionLogTime() const override;
    virtual void setTransactionLogTime(Timestamp value) override;


    virtual void startReceivingNotifications() override;
    virtual void stopReceivingNotifications() override;
    virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
    virtual TransactionMessageBusAdapter* messageBus() const override { return nullptr; }
    virtual QnCommonModule* commonModule() const override { return nullptr; }
protected:
    virtual int dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler) override;
    virtual int dumpDatabaseToFileAsync(const QString& dumpFilePath, ec2::impl::SimpleHandlerPtr) override;
    virtual int restoreDatabaseAsync(const ApiDatabaseDumpData& dbFile, impl::SimpleHandlerPtr handler) override;
private:
    QnConnectionInfo m_connectionInfo;
};

} // namespace ec2
