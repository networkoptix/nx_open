#include "misc_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_license_overflow_data.h"

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiModuleData> &transaction) {
    emit moduleChanged(transaction.params.moduleInformation, transaction.params.isAlive);
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiModuleDataList> &transaction) {
    for (const ApiModuleData &data: transaction.params)
        emit moduleChanged(data.moduleInformation, data.isAlive);
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiSystemNameData> &transaction) {
    emit systemNameChangeRequested(transaction.params.systemName, transaction.params.sysIdTime, transaction.params.tranLogTime);
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::QnMiscManager(QueryProcessorType * const queryProcessor) :
    m_queryProcessor(queryProcessor)
{
}

template<class QueryProcessorType>
QnMiscManager<QueryProcessorType>::~QnMiscManager() {}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::sendModuleInformation(const QnModuleInformationWithAddresses &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformation, isAlive);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::sendModuleInformationList(const QList<QnModuleInformationWithAddresses> &moduleInformationList, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(moduleInformationList);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    auto transaction = prepareTransaction(systemName, sysIdTime, tranLogTime);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}

template<class QueryProcessorType>
QnTransaction<ApiModuleData> QnMiscManager<QueryProcessorType>::prepareTransaction(const QnModuleInformationWithAddresses &moduleInformation, bool isAlive) const {
    QnTransaction<ApiModuleData> transaction(ApiCommand::moduleInfo);
    transaction.params.moduleInformation = moduleInformation;
    transaction.params.isAlive = isAlive;
    transaction.isLocal = true;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiModuleDataList> QnMiscManager<QueryProcessorType>::prepareTransaction(const QList<QnModuleInformationWithAddresses> &moduleInformationList) const {
    QnTransaction<ApiModuleDataList> transaction(ApiCommand::moduleInfoList);

    for (const QnModuleInformation &moduleInformation: moduleInformationList) {
        ApiModuleData data;
        data.moduleInformation = moduleInformation;
        data.isAlive = true;
        transaction.params.push_back(data);
    }
    transaction.isLocal = true;

    return transaction;
}

template<class QueryProcessorType>
QnTransaction<ApiSystemNameData> QnMiscManager<QueryProcessorType>::prepareTransaction(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime) const {
    QnTransaction<ApiSystemNameData> transaction(ApiCommand::changeSystemName);
    transaction.params.systemName = systemName;
    transaction.params.sysIdTime = sysIdTime;
    transaction.params.tranLogTime = tranLogTime;
    return transaction;
}

template<class QueryProcessorType>
int QnMiscManager<QueryProcessorType>::markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) {
    const int reqId = generateRequestID();
    QnTransaction<ApiLicenseOverflowData> transaction(ApiCommand::markLicenseOverflow);
    transaction.params.value = value;
    transaction.params.time = time;
    transaction.isLocal = true;

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync(transaction, [handler, reqId](ErrorCode errorCode){ handler->done(reqId, errorCode); });

    return reqId;
}


template class QnMiscManager<ServerQueryProcessor>;
template class QnMiscManager<FixedUrlClientQueryProcessor>;

} // namespace ec2
