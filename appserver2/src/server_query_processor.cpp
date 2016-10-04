#include  <base_ec2_connection.h>
#include <transaction/transaction.h>

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

ErrorCode detail::ServerQueryProcessor::removeHelper(
    const QnUuid& id,
    ApiCommand::Value command,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend,
    bool notificationNeeded,
    TransactionType::Value transactionType)
{
    QnTransaction<ApiIdData> removeTran = createTransaction(command, ApiIdData(id));
    removeTran.transactionType = transactionType;
    ErrorCode errorCode = processUpdateSync(removeTran, transactionsToSend, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    if (notificationNeeded)
        triggerNotification(connection, removeTran);

    return ErrorCode::ok;
}

ErrorCode detail::ServerQueryProcessor::removeObjAttrHelper(
    const QnUuid& id,
    ApiCommand::Value command,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend)
{
    return removeHelper(id, command, connection, transactionsToSend, true);
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
        QnTransaction<ApiResourceParamWithRefData> removeParamTran = 
            createTransaction(
                ApiCommand::Value::removeResourceParam,
                param);
        triggerNotification(connection, removeParamTran);
    }

    return errorCode;
}

ErrorCode detail::ServerQueryProcessor::removeLayoutsHelper(
    const QnTransaction<ApiIdData>& tran,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend)
{
    ApiObjectInfo userObjectInfo(ApiObjectType::ApiObject_User, tran.params.id);
    ApiObjectInfoList userLayoutsObjInfoList =
        dbManager(m_userAccessData)
            .getNestedObjectsNoLock(userObjectInfo);

    ApiIdDataList userLayouts;
    for (const auto& objInfo : userLayoutsObjInfoList)
        userLayouts.push_back(ApiIdData(objInfo.id));

    ErrorCode errorCode = processMultiUpdateSync(
        ApiCommand::removeLayout,
        tran.transactionType,
        userLayouts,
        transactionsToSend);

    if (errorCode != ErrorCode::ok)
        return errorCode;

    for (const auto& layout: userLayouts)
    {
        QnTransaction<ApiIdData> removeLayoutTran = 
            createTransaction(ApiCommand::Value::removeLayout, layout);
        triggerNotification(connection, removeLayoutTran);
    }

    return errorCode;
}

ErrorCode detail::ServerQueryProcessor::removeObjAccessRightsHelper(
    const QnUuid& id,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend)
{
    return removeHelper(id, ApiCommand::removeAccessRights, connection, transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeResourceStatusHelper(
    const QnUuid& id,
    const AbstractECConnectionPtr& connection,
    std::list<std::function<void()>>* const transactionsToSend,
    TransactionType::Value transactionType)
{
    return removeHelper(
        id,
        ApiCommand::removeResourceStatus,
        connection,
        transactionsToSend,
        false,
        transactionType);
}

} //namespace ec2
