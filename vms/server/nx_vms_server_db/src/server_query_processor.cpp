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
    PostProcessList* const transactionsToSend)
{
    QnTransaction<nx::vms::api::IdData> removeTran = createTransaction(command, nx::vms::api::IdData(id));
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
    PostProcessList* const transactionsToSend)
{
    NX_VERBOSE(this, lit("%1 Processing remove resourse %2 status transaction.")
           .arg(Q_FUNC_INFO)
           .arg(id.toString()));

    return removeHelper(
        id,
        ApiCommand::removeResourceStatus,
        transactionsToSend);
}

detail::ServerQueryProcessor ServerQueryProcessorAccess::getAccess(
    const Qn::UserAccessData userAccessData)
{
    return detail::ServerQueryProcessor(this, userAccessData);
}

namespace detail {

void TransactionExecutor::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    {
        QnMutexLocker lock(&m_mutex);
        m_waitCondition.wakeOne();
    }
}

void TransactionExecutor::enqueData(Command command)
{
    QnMutexLocker lock(&m_mutex);
    if (m_commandQueue.size() >= kMaxQueueSize)
    {
        command.completionHandler(ErrorCode::failure);
        return;
    }
    m_commandQueue.push_back(command);
    m_waitCondition.wakeOne();
}

void TransactionExecutor::run()
{
    std::deque<Command> queue;
    while (!needToStop())
    {
        if (queue.empty())
        {
            QnMutexLocker lock(&m_mutex);
            while (m_commandQueue.empty() && !needToStop())
                m_waitCondition.wait(&m_mutex);
            std::swap(m_commandQueue, queue);
        }

        if (needToStop())
            return;

        QElapsedTimer timer;
        timer.restart();

        ErrorCode result = ErrorCode::ok;
        {
            std::unique_ptr<detail::QnDbManager::QnDbTransactionLocker> dbTran;
            for (int i = 0; i < queue.size(); ++i)
            {
                auto& command = queue[i];
                if (!dbTran && ApiCommand::isPersistent(command.command))
                {
                    dbTran.reset(
                        new detail::QnDbManager::QnDbTransactionLocker(m_db->getTransaction()));
                }

                result = command.result = command.execTranFunc(&command.postProcList);
                if (result != ErrorCode::ok)
                {
                    command.completionHandler(command.result);
                    // Rollback DB transaction and push back unprocessed transactions back to the execution queue
                    QnMutexLocker lock(&m_mutex);
                    for (int j = (int) queue.size() - 1; j > i; --j)
                        m_commandQueue.push_front(queue[j]);
                    queue.erase(queue.begin() + i, queue.end());
                    for (auto& rollbackCommand: queue)
                        rollbackCommand.postProcList.clear();
                    break;
                }
            }
            if (result != ErrorCode::ok)
                continue;

            if (dbTran)
            {
                if (!NX_ASSERT(dbTran->commit()))
                    result = ErrorCode::dbError;
            }
        }

        for (auto& command: queue)
        {
            for (const auto& postProcFunc: command.postProcList)
                postProcFunc();
            command.completionHandler(command.result);
        }
        NX_VERBOSE(this, "Aggregate %1 tranasction. Execution time %2 ms", queue.size(), timer.elapsed());
        queue.clear();
    }
}

} // namespace detail

} //namespace ec2
