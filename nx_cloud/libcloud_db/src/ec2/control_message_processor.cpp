/**********************************************************
* Aug 17, 2016
* a.kolesnikov
***********************************************************/

#include "control_message_processor.h"

#include <functional>

#include "incoming_transaction_dispatcher.h"


namespace nx {
namespace cdb {
namespace ec2 {

ControlMessageProcessor::ControlMessageProcessor(
    TransactionLog* const transactionLog,
    IncomingTransactionDispatcher* const transactionDispatcher)
:
    m_transactionLog(transactionLog),
    m_transactionDispatcher(transactionDispatcher)
{
    using namespace std::placeholders;

    m_transactionDispatcher->registerTransactionHandler
        <::ec2::ApiCommand::tranSyncRequest, ::ec2::ApiSyncRequestData>(
            std::bind(&ControlMessageProcessor::processSyncRequest, this, _1, _2, _3));
    m_transactionDispatcher->registerTransactionHandler
        <::ec2::ApiCommand::tranSyncResponse, ::ec2::QnTranStateResponse>(
            std::bind(&ControlMessageProcessor::processSyncResponse, this, _1, _2, _3));
    m_transactionDispatcher->registerTransactionHandler
        <::ec2::ApiCommand::tranSyncDone, ::ec2::ApiTranSyncDoneData>(
            std::bind(&ControlMessageProcessor::processSyncDone, this, _1, _2, _3));
}

nx::db::DBResult ControlMessageProcessor::processSyncRequest(
    QSqlDatabase* dbConnection,
    const nx::String& systemId,
    ::ec2::ApiSyncRequestData data)
{
    //TODO
    return nx::db::DBResult::statementError;
}

nx::db::DBResult ControlMessageProcessor::processSyncResponse(
    QSqlDatabase* dbConnection,
    const nx::String& systemId,
    ::ec2::QnTranStateResponse data)
{
    //TODO
    return nx::db::DBResult::statementError;
}

nx::db::DBResult ControlMessageProcessor::processSyncDone(
    QSqlDatabase* dbConnection,
    const nx::String& systemId,
    ::ec2::ApiTranSyncDoneData data)
{
    //TODO
    return nx::db::DBResult::statementError;
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
