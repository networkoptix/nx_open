#include  <base_ec2_connection.h>
#include <transaction/transaction.h>

#include "server_query_processor.h"

namespace ec2
{

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
    TransactionPostProcessor* const postProcessor,
    TransactionType::Value transactionType)
{
    QnTransaction<ApiIdData> removeTran = createTransaction(command, ApiIdData(id));
    removeTran.transactionType = transactionType;
    ErrorCode errorCode = processUpdateSync(removeTran, postProcessor, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    return ErrorCode::ok;
}

ErrorCode detail::ServerQueryProcessor::removeObjAttrHelper(
    const QnUuid& id,
    ApiCommand::Value command,
    TransactionPostProcessor* const postProcessor)
{
    return removeHelper(id, command, postProcessor);
}

ErrorCode detail::ServerQueryProcessor::removeObjParamsHelper(
    const QnTransaction<ApiIdData>& tran,
    const AbstractECConnectionPtr& connection,
    TransactionPostProcessor* const postProcessor)
{
    ApiResourceParamWithRefDataList resourceParams;
    dbManager(m_userAccessData).getResourceParamsNoLock(tran.params.id, resourceParams);

    ErrorCode errorCode = processMultiUpdateSync(
        ApiCommand::removeResourceParam,
        tran.transactionType,
        resourceParams,
        postProcessor);

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

ErrorCode detail::ServerQueryProcessor::removeObjAccessRightsHelper(
    const QnUuid& id,
    TransactionPostProcessor* const postProcessor)
{
    return removeHelper(id, ApiCommand::removeAccessRights, postProcessor);
}

ErrorCode detail::ServerQueryProcessor::removeResourceStatusHelper(
    const QnUuid& id,
    TransactionPostProcessor* const postProcessor,
    TransactionType::Value transactionType)
{
    return removeHelper(
        id,
        ApiCommand::removeResourceStatus,
        postProcessor,
        transactionType);
}

} //namespace ec2
