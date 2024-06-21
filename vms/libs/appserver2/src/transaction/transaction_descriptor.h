// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <core/resource_access/user_access_data.h>
#include <nx/utils/type_utils.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/cleanup_db_data.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/license_overflow_data.h>
#include <nx/vms/api/data/lock_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/peer_sync_time_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/runtime_data.h>
#include <nx/vms/api/data/stored_file_data.h>
#include <nx/vms/api/data/system_id_data.h>
#include <nx/vms/api/data/tran_state_data.h>
#include <nx/vms/api/data/update_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/videowall_license_overflow_data.h>
#include <nx/vms/api/data/webpage_data.h>
#include <nx/vms/api/rules/event_info.h>
#include <nx/vms/api/rules/rule.h>
#include <nx_ec/ec_api_fwd.h>
#include <transaction/amend_transaction_data.h>

#include "abstract_persistent_storage.h"
#include "transaction.h"

namespace nx::vms::common { class SystemContext; }

namespace ec2 {

class AbstractECConnection;
class QnLicenseNotificationManager;
class QnResourceNotificationManager;
class QnMediaServerNotificationManager;
class QnCameraNotificationManager;
class QnUserNotificationManager;
class QnTimeNotificationManager;
class QnBusinessEventNotificationManager;
class VmsRulesNotificationManager;
class QnLayoutNotificationManager;
class ShowreelNotificationManager;
class QnVideowallNotificationManager;
class QnWebPageNotificationManager;
class QnStoredFileNotificationManager;
class QnMiscNotificationManager;
class QnDiscoveryNotificationManager;
class AnalyticsNotificationManager;
class LookupListNotificationManager;

enum class RemotePeerAccess
{
    Allowed,
    Forbidden,
    Partial
};

namespace transaction_descriptor {

struct CanModifyStorageData
{
    ErrorCode modifyResourceResult = ErrorCode::failure;
    bool hasExistingStorage;
    nx::vms::api::StorageData request;
    std::function<void(const QString&)> logErrorFunc;
    std::function<nx::vms::api::ResourceData()> getExistingStorageDataFunc;
    QString storageType;
    bool isBackup = false;
};

Result canModifyStorage(const CanModifyStorageData& data);

} // namespace transaction_descriptor

namespace detail {

Result userHasGlobalAccess(nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    nx::vms::api::GlobalPermission permissions);

Result userHasAccess(nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const nx::Uuid& targetResourceOrGroupId,
    nx::vms::api::AccessRights requiredAccess);

Result checkResourceAccess(nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const std::vector<nx::Uuid>& resourceIds,
    nx::vms::api::AccessRight accessRight);

Result checkActionPermission(nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const nx::vms::api::EventActionData& data);

struct NoneType {};

template<typename ParamType>
using CheckSavePermissionFuncType = std::function<Result(
    nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const ParamType&)>;

template<typename ParamType>
using CheckReadPermissionFuncType = std::function<Result(
    nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    ParamType&)>;

template<typename ParamType>
using FilterByReadPermissionFuncType = std::function<void(
    nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    ParamType&)>;

template<typename ParamType>
using CheckRemotePeerAccessFuncType = std::function<RemotePeerAccess(
    nx::vms::common::SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const ParamType&)>;

template<typename ParamType>
using GetTransactionTypeFuncType = std::function<ec2::TransactionType(
    nx::vms::common::SystemContext* systemContext,
    const ParamType&,
    AbstractPersistentStorage*)>;

template<typename ParamType>
using GetHashFuncType = std::function<nx::Uuid(ParamType const &)>;

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
    QnTimeNotificationManager* timeNotificationManager;
    QnBusinessEventNotificationManager* businessEventNotificationManager;
    VmsRulesNotificationManager* vmsRulesNotificationManager;
    QnLayoutNotificationManager* layoutNotificationManager;
    ShowreelNotificationManager* showreelNotificationManager;
    QnVideowallNotificationManager* videowallNotificationManager;
    QnWebPageNotificationManager* webPageNotificationManager;
    QnStoredFileNotificationManager* storedFileNotificationManager;
    QnMiscNotificationManager* miscNotificationManager;
    QnDiscoveryNotificationManager* discoveryNotificationManager;
    AnalyticsNotificationManager* analyticsNotificationManager;
    LookupListNotificationManager* lookupListNotificationManager;
    NotificationSource source;
};

template<typename ParamType>
using TriggerNotificationFuncType = std::function<void(const QnTransaction<ParamType>&, const NotificationParams&)>;

struct TransactionDescriptorBase
{
    ApiCommand::Value value;
    bool isPersistent;
    bool isSystem;
    bool isRemoveOperation;
    QString name;

    ApiCommand::Value getValue() const { return value; }
    const QString& getName() const { return name; }

    TransactionDescriptorBase(
        ApiCommand::Value value,
        bool isPersistent,
        bool isSystem,
        bool isRemoveOperation,
        const char *name)
        :
        value(value),
        isPersistent(isPersistent),
        isSystem(isSystem),
        isRemoveOperation(isRemoveOperation),
        name(QString::fromLatin1(name))
    {}

    virtual ~TransactionDescriptorBase() {}
};

template<typename ParamType>
struct TransactionDescriptor : TransactionDescriptorBase
{
    typedef ParamType paramType;

    GetHashFuncType<ParamType> getHashFunc;
    TriggerNotificationFuncType<ParamType> triggerNotificationFunc;
    CheckSavePermissionFuncType<ParamType> checkSavePermissionFunc;
    CheckReadPermissionFuncType<ParamType> checkReadPermissionFunc;
    FilterByReadPermissionFuncType<ParamType> filterByReadPermissionFunc;
    CheckRemotePeerAccessFuncType<ParamType> checkRemotePeerAccessFunc;
    GetTransactionTypeFuncType<ParamType> getTransactionTypeFunc;

    template<
        typename GetHashF,
        typename TriggerNotificationF,
        typename CheckSavePermissionFunc,
        typename CheckReadPermissionFunc,
        typename FilterByReadPermissionFunc,
        typename CheckRemoteAccessFunc,
        typename GetTransactionTypeFunc
    >
    TransactionDescriptor(ApiCommand::Value value,
        bool isPersistent,
        bool isSystem,
        bool isRemoveOperation,
        const char* name,
        GetHashF&& getHashFunc,
        TriggerNotificationF&& triggerNotificationFunc,
        CheckSavePermissionFunc&& checkSavePermissionFunc,
        CheckReadPermissionFunc&& checkReadPermissionFunc,
        FilterByReadPermissionFunc&& filterByReadPermissionFunc,
        CheckRemoteAccessFunc&& checkRemotePeerAccessFunc,
        GetTransactionTypeFunc&& getTransactionTypeFunc)
        :
        TransactionDescriptorBase(value, isPersistent, isSystem, isRemoveOperation, name),
        getHashFunc(std::forward<GetHashF>(getHashFunc)),
        triggerNotificationFunc(std::forward<TriggerNotificationF>(triggerNotificationFunc)),
        checkSavePermissionFunc(std::forward<CheckSavePermissionFunc>(checkSavePermissionFunc)),
        checkReadPermissionFunc(std::forward<CheckReadPermissionFunc>(checkReadPermissionFunc)),
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
 * Semantics of the transactionHash() function is following:
 * if transaction A follows transaction B and overrides it,
 * their transactionHash() result MUST be the same. Otherwise, transactionHash() result must differ.
 * Obviously, transactionHash() is not needed for the non-persistent transaction.
 */
template<typename Param>
nx::Uuid transactionHash(ApiCommand::Value command, const Param &param)
{
    auto td = ec2::getActualTransactionDescriptorByValue<Param>(command);
    if (!td)
    {
        NX_ASSERT(0, "Transaction descriptor for the given param not found");
        return nx::Uuid();
    }

    return td->getHashFunc(param);
}

} //namespace ec2
