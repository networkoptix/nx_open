#include  <base_ec2_connection.h>
#include "server_query_processor.h"

namespace ec2
{
QnMutex detail::ServerQueryProcessor::m_updateDataMutex;

void detail::ServerQueryProcessor::setAuditData(
    ECConnectionAuditManager* auditManager,
    const QnAuthSession& authSession)
{
    m_auditManager = auditManager;
    m_authSession = authSession;
}

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

    triggerNotification(connection, removeObjAttrTran);

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
        tran.transactionType,
        resourceParams,
        transactionsToSend);

    if (errorCode != ErrorCode::ok)
        return errorCode;

    for (const auto& param : resourceParams)
    {
        QnTransaction<ApiResourceParamWithRefData> removeParamTran(
            ApiCommand::Value::removeResourceParam,
            param);
        triggerNotification(connection, removeParamTran);
    }

    return errorCode;
}

ErrorCode detail::ServerQueryProcessor::removeObjAccessRightsHelper(
    const QnUuid& id,
    const AbstractECConnectionPtr& /*connection*/,
    std::list<std::function<void()>>* const transactionsToSend)
{
    QnTransaction<ApiIdData> removeObjAccessRightsTran(ApiCommand::removeAccessRights, ApiIdData(id));
    ErrorCode errorCode = processUpdateSync(removeObjAccessRightsTran, transactionsToSend, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    return ErrorCode::ok;
}

ErrorCode detail::ServerQueryProcessor::removeResourceStatusHelper(
    const QnUuid& id,
    const AbstractECConnectionPtr& /*connection*/,
    std::list<std::function<void()>>* const transactionsToSend,
    TransactionType::Value transactionType)
{
    QnTransaction<ApiIdData> removeResourceStatusTran(ApiCommand::removeResourceStatus, ApiIdData(id));
    removeResourceStatusTran.transactionType = transactionType;
    ErrorCode errorCode = processUpdateSync(removeResourceStatusTran, transactionsToSend, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    return ErrorCode::ok;
}

} //namespace ec2
