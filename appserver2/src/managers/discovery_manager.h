#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnDiscoveryNotificationManager : public AbstractDiscoveryManagerBase
    {
    public:
        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction);
        void triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction);
        void triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation = true);
        void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran);
    };

    typedef std::shared_ptr<QnDiscoveryNotificationManager> QnDiscoveryNotificationManagerPtr;
    typedef QnDiscoveryNotificationManager *QnDiscoveryNotificationManagerRawPtr;

    template<class QueryProcessorType>
    class QnDiscoveryManager : public AbstractDiscoveryManager
    {
    public:
        QnDiscoveryManager(QnDiscoveryNotificationManagerRawPtr *base, QueryProcessorType * const queryProcessor, const Qn::UserAccessData &userAccessData);
        virtual ~QnDiscoveryManager();

    protected:
        virtual int discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) override;
        virtual int addDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) override;
        virtual int sendDiscoveredServer(const ApiDiscoveredServerData &discoveredServer, impl::SimpleHandlerPtr handler) override;
        virtual int sendDiscoveredServersList(const ApiDiscoveredServerDataList &discoveredServersList, impl::SimpleHandlerPtr handler) override;

    private:
        QnDiscoveryNotificationManagerRawPtr m_base;
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiDiscoveryData> prepareTransaction(ApiCommand::Value command, const QnUuid &id, const QUrl &url, bool ignore) const;
        QnTransaction<ApiDiscoverPeerData> prepareTransaction(const QUrl &url) const;
        QnTransaction<ApiDiscoveredServerData> prepareTransaction(const ApiDiscoveredServerData &discoveredServer) const;
        QnTransaction<ApiDiscoveredServerDataList> prepareTransaction(const ApiDiscoveredServerDataList &discoveredServersList) const;
    };

} // namespace ec2
