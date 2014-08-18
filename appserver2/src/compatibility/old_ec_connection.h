/**********************************************************
* 23 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef OLD_EC_CONNECTION_H
#define OLD_EC_CONNECTION_H

#include <nx_ec/ec_api.h>


namespace ec2
{
    //!Connection to old (python-based) EC. Does not provide any functionality, every method returns \a notImplemented error
    class OldEcConnection
    :
        public AbstractECConnection
    {
    public:
        OldEcConnection(const QnConnectionInfo& connectionInfo);

        virtual QnConnectionInfo connectionInfo() const override;

        virtual AbstractResourceManagerPtr getResourceManager() override;
        virtual AbstractMediaServerManagerPtr getMediaServerManager() override;
        virtual AbstractCameraManagerPtr getCameraManager() override;
        virtual AbstractLicenseManagerPtr getLicenseManager() override;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() override;
        virtual AbstractUserManagerPtr getUserManager() override;
        virtual AbstractLayoutManagerPtr getLayoutManager() override;
        virtual AbstractVideowallManagerPtr getVideowallManager() override;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() override;
        virtual AbstractUpdatesManagerPtr getUpdatesManager() override;
        virtual AbstractMiscManagerPtr getMiscManager() override;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager() override;

        virtual int setPanicMode(Qn::PanicMode value, impl::SimpleHandlerPtr handler) override;
        virtual int getCurrentTime(impl::CurrentTimeHandlerPtr handler) override;
        virtual int dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler) override;
        virtual int restoreDatabaseAsync(const ApiDatabaseDumpData& dbFile, impl::SimpleHandlerPtr handler) override;

        virtual void addRemotePeer(const QUrl& url, const QUuid& peerGuid) override;
        virtual void deleteRemotePeer(const QUrl& url) override;
        virtual void sendRuntimeData(const ec2::ApiRuntimeData &data) override;

        virtual void startReceivingNotifications() override;

    private:
        QnConnectionInfo m_connectionInfo;
    };
}

#endif  //OLD_EC_CONNECTION_H
