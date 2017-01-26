#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnDiscoveryNotificationManager : public AbstractDiscoveryNotificationManager
    {
    public:
        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction, NotificationSource source);
        void triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation = true);
        void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran, NotificationSource source);
    };

    typedef std::shared_ptr<QnDiscoveryNotificationManager> QnDiscoveryNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnDiscoveryManager : public AbstractDiscoveryManager
    {
    public:
        QnDiscoveryManager(QueryProcessorType * const queryProcessor, const Qn::UserAccessData &userAccessData);
        virtual ~QnDiscoveryManager();

    protected:
        virtual int discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) override;
        virtual int addDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) override;
        virtual int sendDiscoveredServer(const ApiDiscoveredServerData &discoveredServer, impl::SimpleHandlerPtr handler) override;
        virtual int sendDiscoveredServersList(const ApiDiscoveredServerDataList &discoveredServersList, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };

} // namespace ec2
