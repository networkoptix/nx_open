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

    protected:
        virtual int discoverPeer(const QnUuid &id, const nx::utils::Url &url, impl::SimpleHandlerPtr handler) override;
        virtual int addDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int removeDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };

    // TODO: Could probably be moved to mediaserver, as it is used only there.
    class QnDiscoveryMonitor: public QObject, public QnCommonModuleAware
    {
    public:
        QnDiscoveryMonitor(TransactionMessageBusAdapter* messageBus);
        virtual ~QnDiscoveryMonitor();

    private:
        void clientFound(QnUuid peerId, Qn::PeerType peerType);
        void serverFound(nx::vms::discovery::ModuleEndpoint module);
        void serverLost(QnUuid id);

        template<typename Transaction, typename Target>
        void send(ApiCommand::Value command, Transaction data, const Target& target);

    private:
        TransactionMessageBusAdapter* m_messageBus;
        std::map<QnUuid, ApiDiscoveredServerData> m_serverCache;
    };

} // namespace ec2
