#ifndef EC2_DISCOVERY_MANAGER_H
#define EC2_DISCOVERY_MANAGER_H

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>
#include <transaction/transaction.h>

namespace ec2 {

    template<class QueryProcessorType>
    class QnDiscoveryManager : public AbstractDiscoveryManager {
    public:
        QnDiscoveryManager(QueryProcessorType * const queryProcessor);
        virtual ~QnDiscoveryManager();

        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction);
        void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &transaction);

    protected:
        virtual int discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) override;
        virtual int addDiscoveryInformation(const QnId &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int removeDiscoveryInformation(const QnId &id, const QList<QUrl> &urls, bool ignore, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiDiscoveryDataList> prepareTransaction(ApiCommand::Value command, const ApiDiscoveryDataList &discoveryDataList) const;
        QnTransaction<ApiDiscoverPeerData> prepareTransaction(const QUrl &url) const;
    };

} // namespace ec2

#endif // EC2_DISCOVERY_MANAGER_H
