#include "misc_manager.h"

#include "utils/network/global_module_finder.h"
#include "utils/network/router.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::QnMiscManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::~QnMiscManager() {}

template<class QueryProcessorType>
void QnMiscManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiModuleData> &transaction) {
    QnModuleInformation moduleInformation;
    QnGlobalModuleFinder::fillFromApiModuleData(transaction.params, &moduleInformation);
    emit moduleChanged(moduleInformation, transaction.params.isAlive, transaction.params.discoverer);
}

template<class QueryProcessorType>
void QnMiscManager<QueryProcessorType>::triggerNotification(const ApiModuleDataList &moduleDataList) {
    foreach (const ApiModuleData &data, moduleDataList) {
        QnModuleInformation moduleInformation;
        QnGlobalModuleFinder::fillFromApiModuleData(data, &moduleInformation);
        emit moduleChanged(moduleInformation, data.isAlive, data.discoverer);
    }
}

template<class QueryProcessorType>
void QnMiscManager<QueryProcessorType>::triggerNotification(const QnTransaction<ApiConnectionData> &transaction) {
    Q_ASSERT_X(transaction.command == ApiCommand::addConnection || transaction.command == ApiCommand::removeConnection, "transaction isn't supported in this function", Q_FUNC_INFO);

    if (transaction.command == ApiCommand::addConnection)
        emit connectionAdded(transaction.params.discovererId, transaction.params.peerId, transaction.params.host, transaction.params.port);
    else
        emit connectionRemoved(transaction.params.discovererId, transaction.params.peerId, transaction.params.host, transaction.params.port);
}

template<class QueryProcessorType>
void QnMiscManager<QueryProcessorType>::triggerNotification(const ApiConnectionDataList &connections) {
    foreach (const ApiConnectionData &connection, connections)
        emit connectionAdded(connection.discovererId, connection.peerId, connection.host, connection.port);
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformation, isAlive);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::addConnection(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(ApiCommand::addConnection, discovererId, peerId, host, port);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::removeConnection(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port, impl::SimpleHandlerPtr handler) {
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
QnTransaction<ApiModuleData> QnMiscManager<QueryProcessorType>::prepareTransaction(const QnModuleInformation &moduleInformation, bool isAlive) const {
    QnTransaction<ApiModuleData> transaction(ApiCommand::moduleInfo, false);
    QnGlobalModuleFinder::fillApiModuleData(moduleInformation, &transaction.params);
    transaction.params.isAlive = isAlive;
    transaction.params.discoverer = QnId(qnCommon->moduleGUID());

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiConnectionData> QnMiscManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port) const {
    QnTransaction<ApiConnectionData> transaction(command, false);
    transaction.params.discovererId = discovererId;
    transaction.params.peerId = peerId;
    transaction.params.host = host;
    transaction.params.port = port;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiConnectionDataList> QnMiscManager<QueryProcessorType>::prepareAvailableConnectionsTransaction() const {
    QnTransaction<ApiConnectionDataList> transaction(ApiCommand::availableConnections, false);

    QMultiHash<QnId, QnRouter::Endpoint> connections = QnRouter::instance()->connections();
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
