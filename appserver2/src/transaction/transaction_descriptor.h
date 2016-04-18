#ifndef __TRANSACTION_DESCRIPTOR_H__
#define __TRANSACTION_DESCRIPTOR_H__

#include <utility>
#include <functional>
#include <utility>
#include <tuple>
#include <type_traits>

#include "transaction.h"

namespace ec2
{
class QnTransactionLog;
typedef std::function<bool (Qn::SerializationFormat, const QByteArray&)> FastFunctionType;

namespace detail
{
struct NoneType {};

template<typename ParamType>
using GetHashFuncType = std::function<QnUuid(ParamType const &)>;

template<typename ParamType>
using SaveTranFuncType = std::function<ErrorCode(const QnTransaction<ParamType> &, QnTransactionLog *)>;

template<typename ParamType>
using SaveSerializedTranFuncType = std::function<ErrorCode(const QnTransaction<ParamType> &, const QByteArray &, QnTransactionLog *)>;

template<typename ParamType>
using CreateTransactionFromAbstractTransactionFuncType = std::function<QnTransaction<ParamType>(const QnAbstractTransaction &tran)>;


class AbstractECConnection;
class QnLicenseNotificationManager;
class QnResourceNotificationManager;
class QnMediaServerNotificationManager;
class QnCameraNotificationManager;
class QnUserNotificationManager;
class QnBusinessEventNotificationManager;
class QnLayoutNotificationManager;
class QnVideowallNotificationManager;
class QnWebPageNotificationManager;
class QnStoredFileNotificationManager;
class QnUpdatesNotificationManager;
class QnMiscNotificationManager;
class QnDiscoveryNotificationManager;

struct NotificationParams
{
    AbstractECConnection *ecConnection;
    QnLicenseNotificationManager *licenseNotificationManager;
    QnResourceNotificationManager *resourceNotificationManager;
    QnMediaServerNotificationManager *mediaServerNotificationManager;
    QnCameraNotificationManager *cameraNotificationManager;
    QnUserNotificationManager *userNotificationManager;
    QnBusinessEventNotificationManager *businessEventNotificationManager;
    QnLayoutNotificationManager *layoutNotificationManager;
    QnVideowallNotificationManager *videowallNotificationManager;
    QnWebPageNotificationManager *webPagerNotificationManager;
    QnStoredFileNotificationManager *storedFileNotificationManager;
    QnUpdatesNotificationManager *updatesNotificationManager;
    QnMiscNotificationManager *miscNotificationManager;
    QnDiscoveryNotificationManager *discoveryNotificationManager;
};

template<typename ParamType>
using TriggerNotificationFuncType = std::function<void(const QnTransaction<ParamType> &, NotificationParams &)>;

template<ApiCommand::Value value, typename ParamType>
struct TransactionDescriptor
{
    constexpr static ApiCommand::Value tag = value;
    typedef ParamType paramType;

    bool isPersistent;
    bool isSystem;
    const char *name;
    GetHashFuncType<ParamType> getHashFunc;
    SaveTranFuncType<ParamType> saveFunc;
    SaveSerializedTranFuncType<ParamType> saveSerializedFunc;
    CreateTransactionFromAbstractTransactionFuncType<ParamType> createTransactionFromAbstractTransactionFunc;
    TriggerNotificationFuncType<ParamType> triggerNotificationFunc;

    TransactionDescriptor(bool isPersistent, bool isSystem, const char *name,
                          GetHashFuncType<ParamType> getHashFunc, SaveTranFuncType<ParamType> saveFunc,
                          SaveSerializedTranFuncType<ParamType> saveSerializedFunc,
                          CreateTransactionFromAbstractTransactionFuncType<ParamType> createTransactionFromAbstractTransactionFunc,
                          TriggerNotificationFuncType<ParamType> triggerNotificationFunc)
        : isPersistent(isPersistent),
          isSystem(isSystem),
          name(name),
          getHashFunc(getHashFunc),
          saveFunc(saveFunc),
          saveSerializedFunc(saveSerializedFunc),
          createTransactionFromAbstractTransactionFunc(createTransactionFromAbstractTransactionFunc),
          triggerNotificationFunc(triggerNotificationFunc)
    {}
};

struct NotDefinedType {};

extern std::tuple<TransactionDescriptor<ApiCommand::NotDefined, NotDefinedType>,
               TransactionDescriptor<ApiCommand::tranSyncRequest, ApiSyncRequestData>,
               TransactionDescriptor<ApiCommand::lockRequest, ApiLockData>,
               TransactionDescriptor<ApiCommand::lockResponse, ApiLockData>,
               TransactionDescriptor<ApiCommand::unlockRequest, ApiLockData>,
               TransactionDescriptor<ApiCommand::peerAliveInfo, ApiPeerAliveData>,
               TransactionDescriptor<ApiCommand::tranSyncDone, ApiTranSyncDoneData>,
               TransactionDescriptor<ApiCommand::testConnection, ApiLoginData>,
               TransactionDescriptor<ApiCommand::connect, ApiLoginData>,
               TransactionDescriptor<ApiCommand::openReverseConnection, ApiReverseConnectionData>
> transactionDescriptors;

/* Compile-time IndexSequence implementation (since stdc++lib shipped with g++ 4.8.2 doesn't have it) */
template<size_t... Indexes> struct IndexSequence {};

template<size_t I, size_t... Tail> struct MakeIndexSequenceImpl
{
    typedef typename MakeIndexSequenceImpl<I-1, I-1, Tail...>::type type;
};

template<size_t... Tail> struct MakeIndexSequenceImpl<0, Tail...>
{
    typedef IndexSequence<Tail...> type;
};

template<size_t Size> struct MakeIndexSequence
{
    typedef typename MakeIndexSequenceImpl<Size - 1, Size - 1>::type type;
};


/* Get transaction descriptor from descriptors tuple in compile time by ApiCommand::Value implementation */
template<ApiCommand::Value value1, typename Descriptor>
struct IsValueEqual;

template<ApiCommand::Value value1, ApiCommand::Value value2, typename T>
struct IsValueEqual<value1, TransactionDescriptor<value2, T>>
{
    static constexpr bool v = (value1 == value2);
};

template<ApiCommand::Value value, size_t I, typename... Args>
constexpr size_t getIndexByValueHelper(const IndexSequence<I>&, const std::tuple<Args...> &)
{
    return I;
}

template<ApiCommand::Value value, size_t I, size_t... Indexes, typename... Args>
constexpr size_t getIndexByValueHelper(const IndexSequence<I, Indexes...> &, const std::tuple<Args...> &tuple)
{
    return IsValueEqual<value, typename std::tuple_element<I, std::tuple<Args...>>::type>::v ?
           I : getIndexByValueHelper<value>(IndexSequence<Indexes...>(), tuple);
}

template<ApiCommand::Value value, size_t... Indexes, typename... Args>
constexpr size_t getIndexByValue(const IndexSequence<Indexes...> &seq, const std::tuple<Args...> &tuple)
{
    return getIndexByValueHelper<value>(seq, tuple);
}

template<ApiCommand::Value value, typename... Args>
constexpr auto getTransactionDescriptorByValueImpl(const std::tuple<Args...> &tuple)
{
    constexpr const size_t I = getIndexByValue<value>(typename MakeIndexSequence<sizeof...(Args)>::type(), tuple);
    static_assert(std::remove_reference<decltype(std::get<I>(tuple))>::type::tag == value, "ApiCommand::Value not found");
    return std::get<I>(tuple);
}


/* Get transaction descriptor from descriptors tuple in compile time by Transaction Paramateres type */
template<typename Params, typename Descriptor>
struct IsParamsEqual;

template<typename Params1, typename Params2, ApiCommand::Value value>
struct IsParamsEqual<Params1, TransactionDescriptor<value, Params2>>
{
    static constexpr bool v = std::is_same<Params1, Params2>::value;
};

template<typename Params, size_t I, typename... Args>
constexpr size_t getIndexByParamsHelper(const IndexSequence<I>&, const std::tuple<Args...> &)
{
    return I;
}

template<typename Params, size_t I, size_t... Indexes, typename... Args>
constexpr size_t getIndexByParamsHelper(const IndexSequence<I, Indexes...> &, const std::tuple<Args...> &tuple)
{
    return IsParamsEqual<Params, typename std::tuple_element<I, std::tuple<Args...>>::type>::v ?
           I : getIndexByParamsHelper<Params>(IndexSequence<Indexes...>(), tuple);
}

template<typename Params, size_t... Indexes, typename... Args>
constexpr size_t getIndexByParams(const IndexSequence<Indexes...> &seq, const std::tuple<Args...> &tuple)
{
    return getIndexByParamsHelper<Params>(seq, tuple);
}

template<typename Params, typename... Args>
constexpr auto getTransactionDescriptorByTransactionParamsImpl(const std::tuple<Args...> &tuple)
{
    constexpr const size_t I = getIndexByParams<Params>(typename MakeIndexSequence<sizeof...(Args)>::type(), tuple);
    static_assert(std::is_same<typename std::remove_reference<decltype(std::get<I>(tuple))>::type::paramType, Params>::value,
                  "Transaction parameter not found");
    return std::get<I>(tuple);
}


/* Get descriptors filtered by Param type */
template <size_t, typename> struct Construct;

template <size_t I, size_t... Indexes>
struct Construct<I, IndexSequence<Indexes...>>
{
    typedef IndexSequence<I, Indexes...> type;
};

template <typename Param, typename IS, typename... Args> struct FilterByParam;

template <typename Param> struct FilterByParam<Param, IndexSequence<>> { typedef IndexSequence<> type; };

template <typename Param, size_t I, size_t... IndexesTail, typename Head, typename ...Tail>
struct FilterByParam<Param, IndexSequence<I, IndexesTail...>, Head, Tail...>
{
    typedef typename std::conditional<IsParamsEqual<Param, Head>::v,
                                      typename Construct<I, typename FilterByParam<Param, IndexSequence<IndexesTail...>, Tail...>::type>::type,
                                      typename FilterByParam<Param, IndexSequence<IndexesTail...>, Tail...>::type >::type type;
};

template<size_t... Indexes, typename Tuple>
constexpr auto getTDsHelper2(const IndexSequence<Indexes...> &, const Tuple &tuple)
{
    return std::make_tuple(std::get<Indexes>(tuple)...);
}

template<typename Param, size_t... Indexes, typename... Args>
constexpr auto getTDsHelper(const IndexSequence<Indexes...> &, const std::tuple<Args...> &tuple)
{
    typedef typename FilterByParam<Param, IndexSequence<Indexes...>, Args...>::type FilteredType;
    return getTDsHelper2(FilteredType(), tuple);
}

template<typename Param, typename... Args>
constexpr auto getTransactionDescriptorsByTransactionParamsImpl(const std::tuple<Args...> &tuple)
{
    return getTDsHelper<Param>(typename MakeIndexSequence<sizeof...(Args)>::type(), tuple);
}

/* Visit TransactionDescriptor with specified ApiCommand::Value tag in runtime implementation*/
template<int I, typename F>
struct DescriptorsVisitor
{
    template<typename... Args>
    static void apply(ApiCommand::Value value, F f, const std::tuple<Args...> &tuple)
    {
        const auto &cur = std::get<I>(tuple);
        if (cur.tag == value)
        {
            f(cur);
            return;
        }
        else
        {
            DescriptorsVisitor<I - 1, F>::apply(value, f, tuple);
        }
    }
};

template<typename F>
struct DescriptorsVisitor<-1, F>
{
    template<typename... Args>
    static void apply(ApiCommand::Value, F, const std::tuple<Args...> &)
    {
        NX_ASSERT(0, "Unknown ApiCommand value");
    }
};

template<typename F, typename... Args>
void visitIfValueImpl(ApiCommand::Value value, F f, const std::tuple<Args...> &tuple)
{
    DescriptorsVisitor<(int)sizeof...(Args) - 1, F>::apply(value, f, tuple);
}

} // namespace detail

template<ApiCommand::Value value>
constexpr auto getTransactionDescriptorByValue()
{
    return detail::getTransactionDescriptorByValueImpl<value>(detail::transactionDescriptors);
}

template<typename Params>
constexpr auto getTransactionDescriptorByTransactionParams()
{
    return detail::getTransactionDescriptorByTransactionParamsImpl<Params>(detail::transactionDescriptors);
}

template<typename Params>
constexpr auto getTransactionDescriptorsFilteredByTransactionParams()
{
    return detail::getTransactionDescriptorsByTransactionParamsImpl<Params>(detail::transactionDescriptors);
}

template<typename F, typename... Args>
void visitTransactionDescriptorIfValue(ApiCommand::Value value, F f)
{
    return detail::visitIfValueImpl(value, f, detail::transactionDescriptors);
}

template<typename F, typename... Args>
void visitTransactionDescriptorIfValue(ApiCommand::Value value, F f, const std::tuple<Args...> &tuple)
{
    return detail::visitIfValueImpl(value, f, tuple);
}

} //namespace ec2

#endif
