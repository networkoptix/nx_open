#include <iostream>
#include "transaction_descriptor.h"
#include "transaction_log.h"
#include "transaction_message_bus.h"
#include "nx_ec/data/api_tran_state_data.h"

namespace ec2
{
namespace detail
{
    template<typename T, typename F>
    ErrorCode saveTransactionImpl(const QnTransaction<T>& tran, ec2::QnTransactionLog *tlog, F f)
    {
        QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
        return tlog->saveToDB(tran, f(tran.params), serializedTran);
    }

    template<typename T, typename F>
    ErrorCode saveSerializedTransactionImpl(const QnTransaction<T>& tran, const QByteArray& serializedTran, QnTransactionLog *tlog, F f)
    {
        return tlog->saveToDB(tran, f(tran.params), serializedTran);
    }

    struct InvalidGetHashHelper
    {
        template<typename Param>
        QnUuid operator ()(const Param &)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!");
            return QnUuid();
        }
    };

    template<typename GetHash>
    struct DefaultSaveTransactionHelper
    {
        template<typename Param>
        ErrorCode operator ()(const QnTransaction<Param> &tran, QnTransactionLog *tlog)
        {
            return saveTransactionImpl(tran, tlog, m_getHash);
        }

        DefaultSaveTransactionHelper(GetHash getHash) : m_getHash(getHash) {}
    private:
        GetHash m_getHash;
    };

    template<typename GetHash>
    constexpr auto createDefaultSaveTransactionHelper(GetHash getHash)
    {
        return DefaultSaveTransactionHelper<GetHash>(getHash);
    }

    template<typename GetHash>
    struct DefaultSaveSerializedTransactionHelper
    {
        template<typename Param>
        ErrorCode operator ()(const QnTransaction<Param> &tran, const QByteArray &serializedTran, QnTransactionLog *tlog)
        {
            return saveSerializedTransactionImpl(tran, serializedTran, tlog, m_getHash);
        }

        DefaultSaveSerializedTransactionHelper(GetHash getHash) : m_getHash(getHash) {}
    private:
        GetHash m_getHash;
    };

    template<typename GetHash>
    constexpr auto createDefaultSaveSerializedTransactionHelper(GetHash getHash)
    {
        return DefaultSaveTransactionHelper<GetHash>(getHash);
    }

    struct InvalidSaveSerializedTransactionHelper
    {
        template<typename Param>
        ErrorCode operator ()(const QnTransaction<Param> &, const QByteArray &, QnTransactionLog *)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
    };

    struct InvalidTriggerNotificationHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &, NotificationParams &)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
    };

#define TRANSACTION_DESCRIPTOR(Key, ParamType, isPersistent, isSystem, getHashFunc, saveFunc, saveSerializedFunc, triggerNotificationFunc) \
    TransactionDescriptor<Key, ParamType>(isPersistent, isSystem, #Key, getHashFunc, saveFunc, saveSerializedFunc, \
                                          [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); }, triggerNotificationFunc)

    std::tuple<TransactionDescriptor<ApiCommand::NotDefined, NotDefinedType>,
               TransactionDescriptor<ApiCommand::tranSyncRequest, ApiSyncRequestData>,
               TransactionDescriptor<ApiCommand::lockRequest, ApiLockData>,
               TransactionDescriptor<ApiCommand::lockResponse, ApiLockData>,
               TransactionDescriptor<ApiCommand::unlockRequest, ApiLockData>> transactionDescriptors =
     std::make_tuple (
        TRANSACTION_DESCRIPTOR(ApiCommand::NotDefined, NotDefinedType,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
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
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::lockRequest, ApiLockData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveSerializedTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::lockResponse, ApiLockData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveSerializedTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::unlockRequest, ApiLockData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveSerializedTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               InvalidTriggerNotificationHelper() // trigger notification
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
