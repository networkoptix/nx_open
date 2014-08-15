#include "misc_manager.h"

#include "utils/network/global_module_finder.h"
#include "utils/network/router.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiModuleData> &transaction) {
    QnModuleInformation moduleInformation;
    QnGlobalModuleFinder::fillFromApiModuleData(transaction.params, &moduleInformation);
    foreach (const QUuid &discovererId, transaction.params.discoverers)
        emit moduleChanged(moduleInformation, transaction.params.isAlive, discovererId);
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiModuleDataList> &transaction) {
    foreach (const ApiModuleData &data, transaction.params) {
        QnModuleInformation moduleInformation;
        QnGlobalModuleFinder::fillFromApiModuleData(data, &moduleInformation);
        foreach (const QUuid &discovererId, data.discoverers)
            emit moduleChanged(moduleInformation, data.isAlive, discovererId);
    }
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiSystemNameData> &transaction) {
    emit systemNameChangeRequested(transaction.params.systemName);
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiConnectionData> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::addConnection || transaction.command == ApiCommand::removeConnection, "transaction isn't supported in this function", Q_FUNC_INFO);

    if (transaction.command == ApiCommand::addConnection)
        emit connectionAdded(transaction.params.discovererId, transaction.params.peerId, transaction.params.host, transaction.params.port);
    else
        emit connectionRemoved(transaction.params.discovererId, transaction.params.peerId, transaction.params.host, transaction.params.port);
}

void QnMiscNotificationManager::triggerNotification(const ApiConnectionDataList &connections) {
    foreach (const ApiConnectionData &connection, connections)
        emit connectionAdded(connection.discovererId, connection.peerId, connection.host, connection.port);
}


template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::QnMiscManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::~QnMiscManager() {}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, const QUuid &discoverer, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformation, isAlive, discoverer);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::sendModuleInformationList(const QList<QnModuleInformation> &moduleInformationList, const QMultiHash<QUuid, QUuid> &discoverersByPeer, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformationList, discoverersByPeer);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::changeSystemName(const QString &systemName, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(systemName);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::addConnection(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::addConnection, discovererId, peerId, host, port);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::removeConnection(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::removeConnection, discovererId, peerId, host, port);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::sendAvailableConnections(impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareAvailableConnectionsTransaction();

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiModuleData> QnMiscManager<QueryProcessorType>::prepareTransaction(const QnModuleInformation &moduleInformation, bool isAlive, const QUuid &discoverer) const {
    QnTransaction<ApiModuleData> transaction(ApiCommand::moduleInfo);
    QnGlobalModuleFinder::fillApiModuleData(moduleInformation, &transaction.params);
    transaction.params.isAlive = isAlive;
    transaction.params.discoverers.append(discoverer);

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiModuleDataList> QnMiscManager<QueryProcessorType>::prepareTransaction(const QList<QnModuleInformation> &moduleInformationList, const QMultiHash<QUuid, QUuid> &discoverersByPeer) const {
    QnTransaction<ApiModuleDataList> transaction(ApiCommand::moduleInfoList);

    foreach (const QnModuleInformation &moduleInformation, moduleInformationList) {
        ApiModuleData data;
        QnGlobalModuleFinder::fillApiModuleData(moduleInformation, &data);
        data.isAlive = true;
        data.discoverers = discoverersByPeer.values(data.id);
        transaction.params.push_back(data);
    }

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiSystemNameData> QnMiscManager<QueryProcessorType>::prepareTransaction(const QString &systemName) const {
    QnTransaction<ApiSystemNameData> transaction(ApiCommand::changeSystemName);
    transaction.params.systemName = systemName;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiConnectionData> QnMiscManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port) const {
    QnTransaction<ApiConnectionData> transaction(command);
    transaction.params.discovererId = discovererId;
    transaction.params.peerId = peerId;
    transaction.params.host = host;
    transaction.params.port = port;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiConnectionDataList> QnMiscManager<QueryProcessorType>::prepareAvailableConnectionsTransaction() const {
    QnTransaction<ApiConnectionDataList> transaction(ApiCommand::availableConnections);

    QMultiHash<QUuid, QnRouter::Endpoint> connections = QnRouter::instance()->connections();
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        ApiConnectionData connection;
        connection.discovererId = it.key();
        connection.peerId = it->id;
        connection.host = it->host;
        connection.port = it->port;
        transaction.params.push_back(connection);
    }

    return transaction;
}

template class QnMiscManager<ServerQueryProcessor>;
template class QnMiscManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
