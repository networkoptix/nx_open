#include "module_information_manager.h"

#include <utils/network/global_module_finder.h>
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

template<class QueryProcessorType>
QnModuleInformationManager<QueryProcessorType>::QnModuleInformationManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnModuleInformationManager<QueryProcessorType>::~QnModuleInformationManager() {}

template<class QueryProcessorType>
void QnModuleInformationManager<QueryProcessorType>::triggerNotification(const QnTransaction<ec2::ApiModuleData> &transaction) {
    QnModuleInformation moduleInformation;
    QnGlobalModuleFinder::fillFromApiModuleData(transaction.params, &moduleInformation);
    emit moduleChanged(moduleInformation, transaction.params.isAlive);
}

template<class QueryProcessorType>
void QnModuleInformationManager<QueryProcessorType>::triggerNotification(const ApiModuleDataList &moduleDataList) {
    foreach (const ApiModuleData &data, moduleDataList) {
        QnModuleInformation moduleInformation;
        QnGlobalModuleFinder::fillFromApiModuleData(data, &moduleInformation);
        emit moduleChanged(moduleInformation, data.isAlive);
    }
}

template<class QueryProcessorType>
int QnModuleInformationManager<QueryProcessorType>::sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformation, isAlive);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiModuleData> QnModuleInformationManager<QueryProcessorType>::prepareTransaction(const QnModuleInformation &moduleInformation, bool isAlive) const {
    QnTransaction<ApiModuleData> transaction(ApiCommand::moduleInfo, false);
    QnGlobalModuleFinder::fillApiModuleData(moduleInformation, &transaction.params);
    transaction.params.isAlive = isAlive;

    return transaction;
}

template class QnModuleInformationManager<ServerQueryProcessor>;
template class QnModuleInformationManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
