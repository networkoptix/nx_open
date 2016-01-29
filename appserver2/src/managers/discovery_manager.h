#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnDiscoveryNotificationManager : public AbstractDiscoveryManager
    {
    public:
        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction);
        void triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction);
        void triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation = true);
        void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran);
        void triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran);
    };

    template<class QueryProcessorType>
    class QnDiscoveryManager : public QnDiscoveryNotificationManager
    {
    public:
        QnDiscoveryManager(QueryProcessorType * const queryProcessor);
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

        QnTransaction<ApiDiscoveryData> prepareTransaction(ApiCommand::Value command, const QnUuid &id, const QUrl &url, bool ignore) const;
        QnTransaction<ApiDiscoverPeerData> prepareTransaction(const QUrl &url) const;
        QnTransaction<ApiDiscoveredServerData> prepareTransaction(const ApiDiscoveredServerData &discoveredServer) const;
        QnTransaction<ApiDiscoveredServerDataList> prepareTransaction(const ApiDiscoveredServerDataList &discoveredServersList) const;
    };

} // namespace ec2
