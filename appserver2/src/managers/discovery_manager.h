#pragma once

#include <common/common_module_aware.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/ec_api.h>
#include <nx/vms/discovery/manager.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnDiscoveryNotificationManager:
        public AbstractDiscoveryNotificationManager,
        public QnCommonModuleAware
    {
    public:
        QnDiscoveryNotificationManager(QnCommonModule* commonModule);

        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction, NotificationSource source);
        void triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation = true);
        void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran, NotificationSource source);
    };

    typedef std::shared_ptr<QnDiscoveryNotificationManager> QnDiscoveryNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnDiscoveryManager: public AbstractDiscoveryManager
    {
    public:
        QnDiscoveryManager(
            QueryProcessorType* const queryProcessor,
            const Qn::UserAccessData &userAccessData);
        virtual ~QnDiscoveryManager();

        virtual void monitorServerDiscovery() override;

    protected:
        virtual int discoverPeer(const QnUuid &id, const QUrl &url, impl::SimpleHandlerPtr handler) override;
        virtual int addDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
        std::unique_ptr<QObject> m_customData;
    };

    // NOTE: Can not be moved in anonimous namespace in cpp because of bug in GCC 4.8.
    class DiscoveryMonitor: public QObject
    {
        Q_OBJECT

    public:
        DiscoveryMonitor(QnTransactionMessageBusBase* messageBus);
        virtual ~DiscoveryMonitor();

    private slots:
        void clientFound(QnUuid peerId, Qn::PeerType peerType);
        void serverFound(nx::vms::discovery::ModuleEndpoint module);
        void serverLost(QnUuid id);

    private:
        template<typename Transaction, typename Target>
        void send(ApiCommand::Value command, Transaction data, const Target& target);

    private:
        QnTransactionMessageBusBase* m_messageBus;
        QnCommonModule* m_common;
        std::map<QnUuid, ApiDiscoveredServerData> m_serverCache;
    };

} // namespace ec2
