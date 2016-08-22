/**********************************************************
* Aug 17, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/buffer.h>

#include <nx_ec/data/api_fwd.h>
#include <nx_ec/data/api_tran_state_data.h>
#include <utils/db/types.h>


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;
class IncomingTransactionDispatcher;

class ControlMessageProcessor
{
public:
    ControlMessageProcessor(
        TransactionLog* const transactionLog,
        IncomingTransactionDispatcher* const transactionDispatcher);

private:
    TransactionLog* const m_transactionLog;
    IncomingTransactionDispatcher* const m_transactionDispatcher;

    nx::db::DBResult processSyncRequest(
        QSqlDatabase* dbConnection,
        const nx::String& systemId,
        ::ec2::ApiSyncRequestData data);

    nx::db::DBResult processSyncResponse(
        QSqlDatabase* dbConnection,
        const nx::String& systemId,
        ::ec2::QnTranStateResponse data);

    nx::db::DBResult processSyncDone(
        QSqlDatabase* dbConnection,
        const nx::String& systemId,
        ::ec2::ApiTranSyncDoneData data);
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
