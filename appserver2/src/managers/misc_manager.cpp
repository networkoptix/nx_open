#include "misc_manager.h"

#include <utils/network/global_module_finder.h>
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
int QnMiscManager<QueryProcessorType>::sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformation, isAlive);

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

template class QnMiscManager<ServerQueryProcessor>;
template class QnMiscManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
