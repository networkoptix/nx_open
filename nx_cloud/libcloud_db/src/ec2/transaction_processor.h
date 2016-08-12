/**********************************************************
* Aug 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/utils/move_only_func.h>

#include <common/common_globals.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction.h>


namespace ec2 {
class QnAbstractTransaction;
}   // namespace ec2

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

class AbstractTransactionProcessor
{
public:
    virtual ~AbstractTransactionProcessor() {}

    /** Parse and process UbJson-serialized transaction */
    virtual bool processTransaction(
        const ::ec2::QnTransactionTransportHeader& transportHeader,
        const ::ec2::QnAbstractTransaction& transaction,
        QnUbjsonReader<QByteArray>* const stream) = 0;
    /** Parse and process Json-serialized transaction */
    virtual bool processTransaction(
        const ::ec2::QnTransactionTransportHeader& transportHeader,
        const ::ec2::QnAbstractTransaction& transaction,
        const QJsonObject& serializedTransactionData) = 0;
};

template<int TransactionCommandValue, typename TransactionDataType>
class TransactionProcessor
:
    public AbstractTransactionProcessor
{
public:
    TransactionProcessor(
        TransactionLog* const transactionLog,
        nx::utils::MoveOnlyFunc<void(TransactionDataType)> processTranFunc)
    :
        m_transactionLog(transactionLog),
        m_processTranFunc(std::move(processTranFunc))
    {
    }

    virtual bool processTransaction(
        const ::ec2::QnTransactionTransportHeader& /*transportHeader*/,
        const ::ec2::QnAbstractTransaction& /*transaction*/,
        QnUbjsonReader<QByteArray>* const /*stream*/)
    {
        //TODO
        return false;
    }

    virtual bool processTransaction(
        const ::ec2::QnTransactionTransportHeader& /*transportHeader*/,
        const ::ec2::QnAbstractTransaction& /*transaction*/,
        const QJsonObject& /*serializedTransactionData*/)
    {
        //TODO
        return false;
    }

private:
    TransactionLog* const m_transactionLog;
    nx::utils::MoveOnlyFunc<void(TransactionDataType)> m_processTranFunc;

    bool processTransaction(
        const ::ec2::QnTransactionTransportHeader& /*transportHeader*/,
        const ::ec2::QnAbstractTransaction& /*transaction*/,
        TransactionDataType /*transactionData*/)
    {
        //TODO
        return false;
    }
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
