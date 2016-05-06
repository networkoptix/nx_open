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
        virtual QString authInfo() const override;

        virtual AbstractResourceManagerPtr getResourceManager() override;
        virtual AbstractMediaServerManagerPtr getMediaServerManager() override;
        virtual AbstractCameraManagerPtr getCameraManager() override;
        virtual AbstractLicenseManagerPtr getLicenseManager() override;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() override;
        virtual AbstractUserManagerPtr getUserManager() override;
        virtual AbstractLayoutManagerPtr getLayoutManager() override;
        virtual AbstractVideowallManagerPtr getVideowallManager() override;
        virtual AbstractWebPageManagerPtr getWebPageManager() override;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() override;
        virtual AbstractUpdatesManagerPtr getUpdatesManager() override;
        virtual AbstractMiscManagerPtr getMiscManager() override;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager() override;
        virtual AbstractTimeManagerPtr getTimeManager() override;

        virtual void addRemotePeer(const QUrl& url) override;
        virtual void deleteRemotePeer(const QUrl& url) override;
        virtual void sendRuntimeData(const ec2::ApiRuntimeData &data) override;

        virtual qint64 getTransactionLogTime() const override;
        virtual void setTransactionLogTime(qint64 value) override;


        virtual void startReceivingNotifications() override;
        virtual void stopReceivingNotifications() override;
        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
    protected:
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int dumpDatabaseToFileAsync( const QString& dumpFilePath, ec2::impl::SimpleHandlerPtr) override;
        virtual int restoreDatabaseAsync( const ApiDatabaseDumpData& dbFile, impl::SimpleHandlerPtr handler ) override;
    private:
        QnConnectionInfo m_connectionInfo;
    };
}

#endif  //OLD_EC_CONNECTION_H
