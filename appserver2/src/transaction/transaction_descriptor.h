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

#include "transaction.h"
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
#include "core/resource/user_resource.h"

#include "managers/business_event_manager.h"
#include "managers/camera_manager.h"
#include "managers/discovery_manager.h"
#include "managers/layout_manager.h"
#include "managers/license_manager.h"
#include "managers/media_server_manager.h"
#include "managers/misc_manager.h"
#include "managers/resource_manager.h"
#include "managers/stored_file_manager.h"
#include "managers/time_manager.h"
#include "managers/updates_manager.h"
#include "managers/user_manager.h"
#include "managers/videowall_manager.h"
#include "managers/webpage_manager.h"

#include "nx/utils/type_utils.h"
#include "transaction/transaction_permissions.h"

namespace ec2
{
class QnTransactionLog;

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
    QnWebPageNotificationManager *webPageNotificationManager;
    QnStoredFileNotificationManager *storedFileNotificationManager;
    QnUpdatesNotificationManager *updatesNotificationManager;
    QnMiscNotificationManager *miscNotificationManager;
    QnDiscoveryNotificationManager *discoveryNotificationManager;
};

template<typename ParamType>
using TriggerNotificationFuncType = std::function<void(const QnTransaction<ParamType> &, const NotificationParams &)>;

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

    template<
        typename GetHashF,
        typename SaveF,
        typename SaveSerializedF,
        typename CreateTranF,
        typename TriggerNotificationF
    >
    TransactionDescriptor(ApiCommand::Value value, bool isPersistent, bool isSystem, const char *name,
                          GetHashF &&getHashFunc, SaveF &&saveFunc, SaveSerializedF &&saveSerializedFunc,
                          CreateTranF &&createTransactionFromAbstractTransactionFunc, TriggerNotificationF &&triggerNotificationFunc)
        :
          TransactionDescriptorBase(value, isPersistent, isSystem, name),
          getHashFunc(std::forward<GetHashF>(getHashFunc)),
          saveFunc(std::forward<SaveF>(saveFunc)),
          saveSerializedFunc(std::forward<SaveSerializedF>(saveSerializedFunc)),
          createTransactionFromAbstractTransactionFunc(std::forward<CreateTranF>(createTransactionFromAbstractTransactionFunc)),
          triggerNotificationFunc(std::forward<TriggerNotificationF>(triggerNotificationFunc))
    {}
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

detail::TransactionDescriptorBase *getTransactionDescriptorByValue(ApiCommand::Value v);
detail::TransactionDescriptorBase *getTransactionDescriptorByName(const QString &s);

} //namespace ec2

#endif
