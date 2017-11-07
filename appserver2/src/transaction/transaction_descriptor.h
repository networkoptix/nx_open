#ifndef __TRANSACTION_DESCRIPTOR_H__
#define __TRANSACTION_DESCRIPTOR_H__

#include <utility>
#include <functional>
#include <utility>
#include <tuple>
#include <type_traits>
#include <cstring>
#include <set>
#include <memory>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <core/resource_access/user_access_data.h>

#include "transaction.h"
#include "nx_ec/access_helpers.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_camera_attributes_data.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_tran_state_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_camera_history_data.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_cleanup_db_data.h"
#include "nx_ec/data/api_update_data.h"
#include "nx_ec/data/api_discovery_data.h"
#include "nx_ec/data/api_routing_data.h"
#include "nx_ec/data/api_system_name_data.h"
#include "nx_ec/data/api_runtime_data.h"
#include "nx_ec/data/api_license_overflow_data.h"
#include "nx_ec/data/api_peer_system_time_data.h"
#include "nx_ec/data/api_webpage_data.h"
#include "nx_ec/data/api_connection_data.h"
#include "nx_ec/data/api_statistics.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "nx_ec/data/api_lock_data.h"

#include "nx/utils/type_utils.h"

class QnCommonModule;

namespace ec2 {

class QnTransactionLog;

class AbstractECConnection;
class QnLicenseNotificationManager;
class QnResourceNotificationManager;
class QnMediaServerNotificationManager;
class QnCameraNotificationManager;
class QnUserNotificationManager;
class QnBusinessEventNotificationManager;
class QnLayoutNotificationManager;
class QnLayoutTourNotificationManager;
class QnVideowallNotificationManager;
class QnWebPageNotificationManager;
class QnStoredFileNotificationManager;
class QnUpdatesNotificationManager;
class QnMiscNotificationManager;
class QnDiscoveryNotificationManager;
namespace detail
{
    class QnDbManager;
}

enum class RemotePeerAccess
{
    Allowed,
    Forbidden,
    Partial
};

namespace detail {

struct NoneType {};

template<typename ParamType>
using CheckSavePermissionFuncType = std::function<bool(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamType&)>;

template<typename ParamType>
using CheckReadPermissionFuncType = std::function<bool(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamType&)>;

template<typename ParamType>
using FilterByReadPermissionFuncType = std::function<void(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamType&)>;

template<typename ParamType>
using FilterBySavePermissionFuncType = std::function<void(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamType&)>;

template<typename ParamType>
using CheckRemotePeerAccessFuncType = std::function<RemotePeerAccess(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamType&)>;

template<typename ParamType>
using GetTransactionTypeFuncType = std::function<ec2::TransactionType::Value(QnCommonModule*, const ParamType&, detail::QnDbManager*)>;

template<typename ParamType>
using GetHashFuncType = std::function<QnUuid(ParamType const &)>;

template<typename ParamType>
using SaveTranFuncType = std::function<ErrorCode(const QnTransaction<ParamType>&, QnTransactionLog*)>;

template<typename ParamType>
using SaveSerializedTranFuncType = std::function<ErrorCode(const QnTransaction<ParamType> &, const QByteArray&, QnTransactionLog*)>;

template<typename ParamType>
using CreateTransactionFromAbstractTransactionFuncType = std::function<QnTransaction<ParamType>(const QnAbstractTransaction& tran)>;

struct NotificationParams
{
    AbstractECConnection* ecConnection;
    QnLicenseNotificationManager* licenseNotificationManager;
    QnResourceNotificationManager* resourceNotificationManager;
    QnMediaServerNotificationManager* mediaServerNotificationManager;
    QnCameraNotificationManager* cameraNotificationManager;
    QnUserNotificationManager* userNotificationManager;
    QnBusinessEventNotificationManager* businessEventNotificationManager;
    QnLayoutNotificationManager* layoutNotificationManager;
    QnLayoutTourNotificationManager* layoutTourNotificationManager;
    QnVideowallNotificationManager* videowallNotificationManager;
    QnWebPageNotificationManager* webPageNotificationManager;
    QnStoredFileNotificationManager* storedFileNotificationManager;
    QnUpdatesNotificationManager* updatesNotificationManager;
    QnMiscNotificationManager* miscNotificationManager;
    QnDiscoveryNotificationManager* discoveryNotificationManager;
    NotificationSource source;
};

template<typename ParamType>
using TriggerNotificationFuncType = std::function<void(const QnTransaction<ParamType>&, const NotificationParams&)>;

struct TransactionDescriptorBase
{
    ApiCommand::Value value;
    bool isPersistent;
    bool isSystem;
    QString name;

    ApiCommand::Value getValue() const { return value; }
    const QString& getName() const { return name; }

    TransactionDescriptorBase(ApiCommand::Value value, bool isPersistent, bool isSystem, const char *name) :
        value(value),
        isPersistent(isPersistent),
        isSystem(isSystem),
        name(name)
    {}

    virtual ~TransactionDescriptorBase() {}
};

template<typename ParamType>
struct TransactionDescriptor : TransactionDescriptorBase
{
    typedef ParamType paramType;

    GetHashFuncType<ParamType> getHashFunc;
    SaveTranFuncType<ParamType> saveFunc;
    SaveSerializedTranFuncType<ParamType> saveSerializedFunc;
    CreateTransactionFromAbstractTransactionFuncType<ParamType> createTransactionFromAbstractTransactionFunc;
    TriggerNotificationFuncType<ParamType> triggerNotificationFunc;
    CheckSavePermissionFuncType<ParamType> checkSavePermissionFunc;
    CheckReadPermissionFuncType<ParamType> checkReadPermissionFunc;
    FilterBySavePermissionFuncType<ParamType> filterBySavePermissionFunc;
    FilterByReadPermissionFuncType<ParamType> filterByReadPermissionFunc;
    CheckRemotePeerAccessFuncType<ParamType> checkRemotePeerAccessFunc;
    GetTransactionTypeFuncType<ParamType> getTransactionTypeFunc;

    template<
		typename GetHashF,
		typename SaveF,
		typename SaveSerializedF,
		typename CreateTranF,
		typename TriggerNotificationF,
		typename CheckSavePermissionFunc,
		typename CheckReadPermissionFunc,
		typename FilterBySavePermissionFunc,
		typename FilterByReadPermissionFunc,
		typename CheckRemoteAccessFunc,
        typename GetTransactionTypeFunc
	>
    TransactionDescriptor(ApiCommand::Value value,
        bool isPersistent,
        bool isSystem,
        const char* name,
        GetHashF&& getHashFunc,
        SaveF&& saveFunc,
        SaveSerializedF&& saveSerializedFunc,
        CreateTranF&& createTransactionFromAbstractTransactionFunc,
        TriggerNotificationF&& triggerNotificationFunc,
        CheckSavePermissionFunc&& checkSavePermissionFunc,
        CheckReadPermissionFunc&& checkReadPermissionFunc,
        FilterBySavePermissionFunc&& filterBySavePermissionFunc,
        FilterByReadPermissionFunc&& filterByReadPermissionFunc,
        CheckRemoteAccessFunc&& checkRemotePeerAccessFunc,
        GetTransactionTypeFunc&& getTransactionTypeFunc)
        :
        TransactionDescriptorBase(value, isPersistent, isSystem, name),
        getHashFunc(std::forward<GetHashF>(getHashFunc)),
        saveFunc(std::forward<SaveF>(saveFunc)),
        saveSerializedFunc(std::forward<SaveSerializedF>(saveSerializedFunc)),
        createTransactionFromAbstractTransactionFunc(std::forward<CreateTranF>(createTransactionFromAbstractTransactionFunc)),
        triggerNotificationFunc(std::forward<TriggerNotificationF>(triggerNotificationFunc)),
        checkSavePermissionFunc(std::forward<CheckSavePermissionFunc>(checkSavePermissionFunc)),
        checkReadPermissionFunc(std::forward<CheckReadPermissionFunc>(checkReadPermissionFunc)),
        filterBySavePermissionFunc(std::forward<FilterBySavePermissionFunc>(filterBySavePermissionFunc)),
        filterByReadPermissionFunc(std::forward<FilterByReadPermissionFunc>(filterByReadPermissionFunc)),
        checkRemotePeerAccessFunc(std::forward<CheckRemoteAccessFunc>(checkRemotePeerAccessFunc)),
        getTransactionTypeFunc(std::forward<GetTransactionTypeFunc>(getTransactionTypeFunc))
    {
    }
};

typedef std::shared_ptr<TransactionDescriptorBase> DescriptorBasePtr;

typedef boost::multi_index_container<
    DescriptorBasePtr,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            boost::multi_index::const_mem_fun<
                TransactionDescriptorBase,
                ec2::ApiCommand::Value,
                &TransactionDescriptorBase::getValue
            >
        >,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::const_mem_fun<
                TransactionDescriptorBase,
                const QString&,
                &TransactionDescriptorBase::getName
            >
        >
    >
> DescriptorBaseContainer;

extern DescriptorBaseContainer transactionDescriptors;

} //namespace detail

detail::TransactionDescriptorBase* getTransactionDescriptorByValue(ApiCommand::Value v);
detail::TransactionDescriptorBase* getTransactionDescriptorByName(const QString& name);

template<typename Param>
detail::TransactionDescriptor<Param>* getActualTransactionDescriptorByValue(ApiCommand::Value command)
{
    auto tdBase = ec2::getTransactionDescriptorByValue(command);
    NX_ASSERT(tdBase);
    auto td = dynamic_cast<detail::TransactionDescriptor<Param>*>(tdBase);
    NX_ASSERT(td);
    return td;
}

template<typename Param>
detail::TransactionDescriptor<Param>* getTransactionDescriptorByTransaction(const QnTransaction<Param>& tran)
{
    return getActualTransactionDescriptorByValue<Param>(tran.command);
}

/**
* For this function to work properly all transaction descriptors for the same api data structures should have
* same Access Rights checker functions. For example setResourceParam and getResourceParam have the same checker for
* read access - ReadResourceParamAccess.
*/
template<typename Param>
detail::TransactionDescriptor<Param>* getTransactionDescriptorByParam()
{
    static std::atomic<detail::TransactionDescriptor<Param>*> holder(nullptr);
    if (!holder.load())
    {
        for (auto it = detail::transactionDescriptors.get<0>().begin(); it != detail::transactionDescriptors.get<0>().end(); ++it)
        {
            auto tdBase = (*it).get();
            auto td = dynamic_cast<detail::TransactionDescriptor<Param>*>(tdBase);
            if (td)
                holder = td;
        }
    }
    NX_ASSERT(holder.load(), "Transaciton descriptor for the given param not found");
    return holder;
}

/**
* Semantics of the transactionHash() function is following:
* if transaction A follows transaction B and overrides it,
* their transactionHash() result MUST be the same. Otherwise, transactionHash() result must differ.
* Obviously, transactionHash() is not needed for the non-persistent transaction.
*/

template<typename Param>
static QnUuid transactionHash(ApiCommand::Value command, const Param &param)
{
    auto td = ec2::getActualTransactionDescriptorByValue<Param>(command);
    if (!td)
    {
        NX_ASSERT(0, "Transaction descriptor for the given param not found");
        return QnUuid();
    }

    return td->getHashFunc(param);
}

} //namespace ec2

#endif
