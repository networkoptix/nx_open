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
    PostProcessList* const transactionsToSend,
    TransactionType::Value transactionType)
{
    QnTransaction<nx::vms::api::IdData> removeTran = createTransaction(command, nx::vms::api::IdData(id));
    removeTran.transactionType = transactionType;
    ErrorCode errorCode = processUpdateSync(removeTran, transactionsToSend, 0);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    return ErrorCode::ok;
}

ErrorCode detail::ServerQueryProcessor::removeObjAttrHelper(
    const QnUuid& id,
    ApiCommand::Value command,
    PostProcessList* const transactionsToSend)
{
    return removeHelper(id, command, transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeObjParamsHelper(
    const QnTransaction<nx::vms::api::IdData>& tran,
    const AbstractECConnectionPtr& /*connection*/,
    PostProcessList* const transactionsToSend)
{
    nx::vms::api::ResourceParamWithRefDataList resourceParams;
    m_db.getResourceParamsNoLock(tran.params.id, resourceParams);

    return processMultiUpdateSync(
        ApiCommand::removeResourceParam,
        tran.transactionType,
        resourceParams,
        transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeUserAccessRightsHelper(
    const QnUuid& id,
    PostProcessList* const transactionsToSend)
{
    return removeHelper(id, ApiCommand::removeAccessRights, transactionsToSend);
}

ErrorCode detail::ServerQueryProcessor::removeResourceStatusHelper(
    const QnUuid& id,
    PostProcessList* const transactionsToSend,
    TransactionType::Value transactionType)
{
    NX_LOG(lit("%1 Processing remove resourse %2 status transaction. Transaction type = %3")
           .arg(Q_FUNC_INFO)
           .arg(id.toString())
           .arg(transactionType), cl_logDEBUG2);

    return removeHelper(
        id,
        ApiCommand::removeResourceStatus,
        transactionsToSend,
        transactionType);
}

detail::ServerQueryProcessor ServerQueryProcessorAccess::getAccess(
    const Qn::UserAccessData userAccessData)
{
    return detail::ServerQueryProcessor(this, userAccessData);
}

} //namespace ec2
