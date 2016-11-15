#include  <base_ec2_connection.h>
#include <transaction/transaction.h>

#include "server_query_processor.h"

namespace ec2
{
QnMutex detail::ServerQueryProcessor::m_updateDataMutex(QnMutex::Recursive);

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
    std::list<std::function<void()>>* const transactionsToSend,
    TransactionType::Value transactionType)
{
    QnTransaction<ApiIdData> removeTran = createTransaction(command, ApiIdData(id));
    removeTran.transactionType = transactionType;
    ErrorCode errorCode = processUpdateSync(removeTran, transactionsToSend, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    return ErrorCode::ok;
}

ErrorCode detail::ServerQueryProcessor::removeObjAttrHelper(
    const QnUuid& id,
    ApiCommand::Value command,
    std::list<std::function<void()>>* const transactionsToSend)
{
    return removeHelper(id, command, transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeObjParamsHelper(
    const QnTransaction<ApiIdData>& tran,
    const AbstractECConnectionPtr& /*connection*/,
    std::list<std::function<void()>>* const transactionsToSend)
{
    ApiResourceParamWithRefDataList resourceParams;
    dbManager(m_userAccessData).getResourceParamsNoLock(tran.params.id, resourceParams);

    return processMultiUpdateSync(
        ApiCommand::removeResourceParam,
        tran.transactionType,
        resourceParams,
        transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeObjAccessRightsHelper(
    const QnUuid& id,
    std::list<std::function<void()>>* const transactionsToSend)
{
    return removeHelper(id, ApiCommand::removeAccessRights, transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeResourceStatusHelper(
    const QnUuid& id,
    std::list<std::function<void()>>* const transactionsToSend,
    TransactionType::Value transactionType)
{
    return removeHelper(
        id,
        ApiCommand::removeResourceStatus,
        transactionsToSend,
        transactionType);
}

} //namespace ec2
