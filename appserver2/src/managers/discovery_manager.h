#ifndef EC2_DISCOVERY_MANAGER_H
#define EC2_DISCOVERY_MANAGER_H

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>
#include <transaction/transaction.h>

namespace ec2 {

    class QnDiscoveryNotificationManager : public AbstractDiscoveryManager {
    public:
        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction);
        void triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction);
        void triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation = true);
    };

    template<class QueryProcessorType>
    class QnDiscoveryManager : public QnDiscoveryNotificationManager {
    public:
        QnDiscoveryManager(QueryProcessorType * const queryProcessor);
        virtual ~QnDiscoveryManager();

    protected:
        virtual int discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) override;
        virtual int addDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;
        virtual int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiDiscoveryData> prepareTransaction(ApiCommand::Value command, const QnUuid &id, const QUrl &url, bool ignore) const;
        QnTransaction<ApiDiscoverPeerData> prepareTransaction(const QUrl &url) const;
    };

} // namespace ec2

#endif // EC2_DISCOVERY_MANAGER_H
