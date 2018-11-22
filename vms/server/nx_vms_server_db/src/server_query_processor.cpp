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
    NX_VERBOSE(this, lit("%1 Processing remove resourse %2 status transaction. Transaction type = %3")
           .arg(Q_FUNC_INFO)
           .arg(id.toString())
           .arg(transactionType));

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


void ServerQueryProcessorAccess::stop()
{
    pleaseStop();
    m_waitCondition.wakeOne();
    QnLongRunnable::stop();
    Ec2ThreadPool::instance()->clear();
    Ec2ThreadPool::instance()->waitForDone();
}

void ServerQueryProcessorAccess::run()
{
    while (!needToStop())
    {
        std::vector<Command> queue;
        {
            QnMutexLocker lock(&m_updateMutex);
            while (m_commandQueue.empty() && !needToStop())
                m_waitCondition.wait(&m_updateMutex);
            std::swap(m_commandQueue, queue);
        }

        ErrorCode result = ErrorCode::ok;
        detail::QnDbManager::QnDbTransactionLocker transaction(m_db->getTransaction());
        {
            for (auto& command: queue)
            {
                result = command.result = command.execTranFunc(&command.postProcList);
                if (result == ErrorCode::dbError)
                    break;
            }
            if (result != ErrorCode::dbError)
                transaction.commit();
        }

        for (auto& command: queue)
        {
            if (result == ErrorCode::dbError)
                command.result = ErrorCode::dbError; //< Mark all transactions as error because data is rollback.
            if (command.result == ErrorCode::ok)
            {
                for (const auto& postProcFunc: command.postProcList)
                    postProcFunc();
            }
            command.execHandler(command.result);
        }
    }
}

} //namespace ec2
