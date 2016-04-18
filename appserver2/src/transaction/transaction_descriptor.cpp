#include <iostream>
#include "transaction_descriptor.h"
#include "transaction_log.h"
#include "transaction_message_bus.h"
#include "nx_ec/data/api_tran_state_data.h"

namespace ec2
{
namespace detail
{
    template <class T>
    ErrorCode saveTransactionImpl(const QnTransaction<T>& tran, ec2::QnTransactionLog *tlog)
    {
        QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
        return tlog->saveToDB(tran, tlog->transactionHash(tran.params), serializedTran);
    }

    template <class T>
    ErrorCode saveSerializedTransactionImpl(const QnTransaction<T>& tran, const QByteArray& serializedTran, QnTransactionLog *tlog)
    {
        return tlog->saveToDB(tran, tlog->transactionHash(tran.params), serializedTran);
    }

#define TRANSACTION_DESCRIPTOR(Key, ParamType, isPersistent, isSystem, getHashFunc, saveFunc, saveSerializedFunc, triggerNotificationFunc) \
    TransactionDescriptor<Key, ParamType>(isPersistent, isSystem, #Key, getHashFunc, saveFunc, saveSerializedFunc, [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); }, triggerNotificationFunc)

    std::tuple<TransactionDescriptor<ApiCommand::NotDefined, NotDefinedType>,
           TransactionDescriptor<ApiCommand::tranSyncRequest, ApiSyncRequestData>> transactionDescriptors =
     std::make_tuple (
        TRANSACTION_DESCRIPTOR(ApiCommand::NotDefined, NotDefinedType,
                               true, // persistent
                               false, // system
                               [] (const NotDefinedType &) {return QnUuid();}, // getHash
                               [] (const QnTransaction<NotDefinedType> &, QnTransactionLog *) { return ErrorCode::notImplemented; }, // save
                               [] (const QnTransaction<NotDefinedType> &, const QByteArray &, QnTransactionLog *) { return ErrorCode::notImplemented; }, // save serialized
                               [] (const QnTransaction<NotDefinedType> &, NotificationParams &)  // trigger notification
                                  {
                                      NX_ASSERT(0, Q_FUNC_INFO, "No notification for this transaction");
                                  }
                               ),
        TRANSACTION_DESCRIPTOR(ApiCommand::tranSyncRequest, ApiSyncRequestData,
                               false, // persistent
                               true, // system
                               [] (const ApiSyncRequestData &) { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }, // getHash
                               [] (const QnTransaction<ApiSyncRequestData> &tran, QnTransactionLog *tlog) { return saveTransactionImpl(tran, tlog); }, // save
                               [] (const QnTransaction<ApiSyncRequestData> &tran, const QByteArray &serializedTran, QnTransactionLog *tlog) // save serialized
                                  { return saveSerializedTransactionImpl(tran, serializedTran, tlog); },
                               [] (const QnTransaction<NotDefinedType> &, NotificationParams &) // trigger notification
                                  {
                                      NX_ASSERT(0, Q_FUNC_INFO, "No notification for this transaction");
                                  }
                              )

    );
#undef TRANSACTION_DESCRIPTOR
}
}

struct GenericVisitor
{
    template<typename Desc>
    void operator()(const Desc &desc)
    {
        std::cout << desc.isPersistent;
    }
};

int foo()
{
    std::cout << ec2::getTransactionDescriptorByValue<ec2::ApiCommand::Value::NotDefined>().isPersistent;
    std::cout << ec2::getTransactionDescriptorByTransactionParams<ec2::ApiSyncRequestData>().isPersistent;
    auto tuple = ec2::getTransactionDescriptorsFilteredByTransactionParams<ec2::ApiSyncRequestData>();
    ec2::visitTransactionDescriptorIfValue(ec2::ApiCommand::addLicense, GenericVisitor(), tuple);
    ec2::visitTransactionDescriptorIfValue(ec2::ApiCommand::NotDefined, GenericVisitor());
    return 1;
}
