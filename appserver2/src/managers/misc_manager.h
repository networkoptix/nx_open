#ifndef MODULE_INFORMATION_MANAGER_H
#define MODULE_INFORMATION_MANAGER_H

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_module_data.h>
#include <nx_ec/data/api_connection_data.h>
#include <nx_ec/data/api_system_name_data.h>
#include <transaction/transaction.h>

namespace ec2 {

    class QnMiscNotificationManager : public AbstractMiscManager {
    public:
        void triggerNotification(const QnTransaction<ApiModuleData> &transaction);
        void triggerNotification(const QnTransaction<ApiModuleDataList> &transaction);
        void triggerNotification(const QnTransaction<ApiSystemNameData> &transaction);
        void triggerNotification(const QnTransaction<ApiConnectionData> &transaction);
        void triggerNotification(const ApiConnectionDataList &connections);
    };

    template<class QueryProcessorType>
    class QnMiscManager : public QnMiscNotificationManager {
    public:
        QnMiscManager(QueryProcessorType * const queryProcessor);
        virtual ~QnMiscManager();

    protected:
        virtual int sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, const QnId &discoverer, impl::SimpleHandlerPtr handler) override;
        virtual int sendModuleInformationList(const QList<QnModuleInformation> &moduleInformationList, const QMultiHash<QnId, QnId> &discoverersByPeer, impl::SimpleHandlerPtr handler) override;
        virtual int changeSystemName(const QString &systemName, impl::SimpleHandlerPtr handler) override;
        virtual int addConnection(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port, impl::SimpleHandlerPtr handler) override;
        virtual int removeConnection(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port, impl::SimpleHandlerPtr handler) override;
        virtual int sendAvailableConnections(impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiModuleData> prepareTransaction(const QnModuleInformation &moduleInformation, bool isAlive, const QnId &discoverer) const;
        QnTransaction<ApiModuleDataList> prepareTransaction(const QList<QnModuleInformation> &moduleInformationList, const QMultiHash<QnId, QnId> &discoverersByPeer) const;
        QnTransaction<ApiSystemNameData> prepareTransaction(const QString &systemName) const;
        QnTransaction<ApiConnectionData> prepareTransaction(ApiCommand::Value command, const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port) const;
        QnTransaction<ApiConnectionDataList> prepareAvailableConnectionsTransaction() const;
    };

} // namespace ec2

#endif // MODULE_INFORMATION_MANAGER_H
