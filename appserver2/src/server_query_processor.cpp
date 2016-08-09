#include  <base_ec2_connection.h>
#include "server_query_processor.h"

namespace ec2
{
QnMutex detail::ServerQueryProcessor::m_updateDataMutex;

ErrorCode detail::ServerQueryProcessor::removeObjAttrHelper(
    const QnUuid& id,
    ApiCommand::Value command,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend)
{
    QnTransaction<ApiIdData> removeObjAttrTran(command, ApiIdData(id));
    ErrorCode errorCode = processUpdateSync(removeObjAttrTran, transactionsToSend, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    connection
        ->notificationManager()
        ->triggerNotification(removeObjAttrTran);

    return ErrorCode::ok;
}

ErrorCode detail::ServerQueryProcessor::removeObjParamsHelper(
    const QnTransaction<ApiIdData>& tran,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend)
{
    ApiResourceParamWithRefDataList resourceParams;
    dbManager(m_userAccessData).getResourceParamsNoLock(tran.params.id, resourceParams);

    ErrorCode errorCode = processMultiUpdateSync(
        ApiCommand::removeResourceParam,
        tran.isLocal,
        resourceParams,
        transactionsToSend);

    if (errorCode != ErrorCode::ok)
        return errorCode;

    for (const auto& param : resourceParams)
    {
        QnTransaction<ApiResourceParamWithRefData> removeParamTran(
            ApiCommand::Value::removeResourceParam,
            param);
        connection
            ->notificationManager()
            ->triggerNotification(removeParamTran);
    }

    return errorCode;
}
} //namespace ec2
