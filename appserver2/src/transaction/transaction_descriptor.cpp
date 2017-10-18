#include "transaction_descriptor.h"
#include "transaction_message_bus.h"

#include <iostream>

#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource_access/user_access_data.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/param.h>

#include <utils/license_usage_helper.h>

#include <nx_ec/data/api_tran_state_data.h>

#include "managers/business_event_manager.h"
#include "managers/camera_manager.h"
#include "managers/discovery_manager.h"
#include "managers/layout_manager.h"
#include <managers/layout_tour_manager.h>
#include "managers/license_manager.h"
#include "managers/media_server_manager.h"
#include "managers/misc_manager.h"
#include "managers/resource_manager.h"
#include "managers/stored_file_manager.h"
#include "managers/updates_manager.h"
#include "managers/user_manager.h"
#include "managers/videowall_manager.h"
#include "managers/webpage_manager.h"
#include <database/db_manager.h>

namespace ec2 {
namespace access_helpers {

namespace detail {
std::vector<QString> getRestrictedKeysByMode(Mode mode)
{
    using namespace nx::settings_names;

    switch (mode)
    {
        case Mode::read:
            return {
                kNamePassword,
                ldapAdminPassword,
                kNameCloudAuthKey,
                Qn::CAMERA_CREDENTIALS_PARAM_NAME,
                Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME
            };
        case Mode::write:
            return {
                kNameCloudSystemId,
                kNameCloudAuthKey,
                kCloudHostName,
                kNameLocalSystemId
            };
    }

    return std::vector<QString>();
}
}

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, KeyValueFilterType* keyValue)
{
    auto in_ = [] (const std::vector<QString>& vec, const QString& value)
    {
        return std::any_of(vec.cbegin(), vec.cend(), [&value](const QString& s) { return s == value; });
    };

    if (in_(detail::getRestrictedKeysByMode(mode), keyValue->first) && accessData != Qn::kSystemAccess)
        return false;

    return true;
}

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, const QString& key)
{
    QString dummy = lit("dummy");

    KeyValueFilterType kv(key, &dummy);
    return kvSystemOnlyFilter(mode, accessData, &kv);
}

bool kvSystemOnlyFilter(Mode mode, const Qn::UserAccessData& accessData, const QString& key, QString* value)
{
    KeyValueFilterType kv(key, value);
    return kvSystemOnlyFilter(mode, accessData, &kv);
}

void applyValueFilters(
    Mode mode,
    const Qn::UserAccessData& accessData,
    KeyValueFilterType* keyValue,
    const FilterFunctorListType& filterList,
    bool* allowed)
{
    if (allowed)
        *allowed = true;

    for (auto filter : filterList)
    {
        bool isAllowed = filter(mode, accessData, keyValue);

        if (allowed && !isAllowed)
            *allowed = false;
    }
}

}

namespace detail {

template<typename T, typename F>
ErrorCode saveTransactionImpl(const QnTransaction<T>& tran, ec2::QnTransactionLog *tlog, F f)
{
    QByteArray serializedTran = tlog->serializer()->serializedTransaction(tran);
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
        // NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!");
        return QnUuid();
    }
};

template<typename GetHash>
struct DefaultSaveTransactionHelper
{
    template<typename Param>
    ErrorCode operator ()(const QnTransaction<Param>& tran, QnTransactionLog* tlog)
    {
        return saveTransactionImpl(tran, tlog, m_getHash);
    }

    DefaultSaveTransactionHelper(GetHash getHash): m_getHash(getHash) {}
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

    DefaultSaveSerializedTransactionHelper(GetHash getHash): m_getHash(getHash) {}
private:
    GetHash m_getHash;
};

template<typename GetHash>
constexpr auto createDefaultSaveSerializedTransactionHelper(GetHash getHash)
{
    return DefaultSaveSerializedTransactionHelper<GetHash>(getHash);
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
    void operator ()(const QnTransaction<Param> &, const NotificationParams &)
    {
        NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
    }
};

struct CreateHashByIdHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &param) { return param.id; }
};

struct CreateHashByUserIdHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &param) { return param.userId; }
};

struct CreateHashByIdRfc4122Helper
{
    CreateHashByIdRfc4122Helper(const QByteArray additionalData = QByteArray()) :
        m_additionalData(additionalData)
    {}

    template<typename Param>
    QnUuid operator ()(const Param &param) \
    {
        if (m_additionalData.isNull())
            return QnTransactionLog::makeHash(param.id.toRfc4122());
        return QnTransactionLog::makeHash(param.id.toRfc4122(), m_additionalData);
    }

private:
    QByteArray m_additionalData;
};

struct CreateHashForResourceParamWithRefDataHelper
{
    QnUuid operator ()(const ApiResourceParamWithRefData &param)
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData("res_params");
        hash.addData(param.resourceId.toRfc4122());
        hash.addData(param.name.toUtf8());
        return QnUuid::fromRfc4122(hash.result());
    }
};

void apiIdDataTriggerNotificationHelper(const QnTransaction<ApiIdData> &tran, const NotificationParams &notificationParams)
{
    switch (tran.command)
    {
        case ApiCommand::removeServerUserAttributes:
            return notificationParams.mediaServerNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeResource:
        case ApiCommand::removeResourceStatus:
            return notificationParams.resourceNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeCamera:
            return notificationParams.cameraNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeStorage:
            return notificationParams.mediaServerNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeUser:
        case ApiCommand::removeUserRole:
            return notificationParams.userNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeEventRule:
            return notificationParams.businessEventNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeLayout:
            return notificationParams.layoutNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeLayoutTour:
            return notificationParams.layoutTourNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeVideowall:
            return notificationParams.videowallNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeWebPage:
            return notificationParams.webPageNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeCameraUserAttributes:
            return notificationParams.cameraNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::forcePrimaryTimeServer:
        case ApiCommand::removeAccessRights:
            //#ak no notification needed
            break;
        default:
            NX_ASSERT(false);
    }
}

QnUuid createHashForServerFootageDataHelper(const ApiServerFootageData &params)
{
    return QnTransactionLog::makeHash(params.serverGuid.toRfc4122(), "history");
}

QnUuid createHashForApiCameraAttributesDataHelper(const ApiCameraAttributesData &params)
{
    return QnTransactionLog::makeHash(params.cameraId.toRfc4122(), "camera_attributes");
}

QnUuid createHashForApiAccessRightsDataHelper(const ApiAccessRightsData& params)
{
    return QnTransactionLog::makeHash(params.userId.toRfc4122(), "access_rights");
}

QnUuid createHashForApiLicenseDataHelper(const ApiLicenseData &params)
{
    return QnTransactionLog::makeHash(params.key, "ApiLicense");
}

QnUuid createHashForApiMediaServerUserAttributesDataHelper(const ApiMediaServerUserAttributesData &params)
{
    return QnTransactionLog::makeHash(params.serverId.toRfc4122(), "server_attributes");
}

QnUuid createHashForApiStoredFileDataHelper(const ApiStoredFileData &params)
{
    return QnTransactionLog::makeHash(params.path.toUtf8());
}

QnUuid createHashForApiDiscoveryDataHelper(const ApiDiscoveryData& params)
{
    return QnTransactionLog::makeHash("discovery_data", params);
}

struct CameraNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.cameraNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct ResourceNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.resourceNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct MediaServerNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.mediaServerNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct UserNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.userNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct LayoutNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.layoutNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct LayoutTourNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.layoutTourNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct VideowallNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.videowallNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct BusinessEventNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.businessEventNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct StoredFileNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.storedFileNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct LicenseNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.licenseNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct UpdateNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.updatesNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct DiscoveryNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.discoveryNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct WebPageNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.webPageNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct EmptyNotificationHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &, const NotificationParams &) {}
};

void apiIdDataListTriggerNotificationHelper(const QnTransaction<ApiIdDataList> &tran, const NotificationParams &notificationParams)
{
    switch (tran.command)
    {
        case ApiCommand::removeStorages:
            return notificationParams.mediaServerNotificationManager->triggerNotification(tran, notificationParams.source);
        case ApiCommand::removeResources:
            return notificationParams.resourceNotificationManager->triggerNotification(tran, notificationParams.source);
        default:
            NX_ASSERT(false);
    }
}

// Permission helpers
struct InvalidAccess
{
    template<typename Param>
    bool operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&)
    {
        auto td = getTransactionDescriptorByParam<Param>();
        auto transactionName = td->getName();
        NX_ASSERT(0, lit("Invalid access check for this transaction (%1). We shouldn't be here.").arg(transactionName));
        return false;
    }
};

struct InvalidAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData&, const Param&)
    {
        auto td = getTransactionDescriptorByParam<Param>();
        auto transactionName = td->getName();
        NX_ASSERT(0, lit("Invalid outgoing transaction access check (%1). We shouldn't be here.").arg(transactionName));
        return RemotePeerAccess::Forbidden;
    }
};

bool hasSystemAccess(const Qn::UserAccessData& accessData)
{
    return accessData.access == Qn::UserAccessData::Access::System;
}

struct SystemSuperUserAccessOnly
{
    template<typename Param>
    bool operator()(QnCommonModule*, const Qn::UserAccessData& accessData, const Param&)
    {
        return hasSystemAccess(accessData);
    }
};

struct SystemSuperUserAccessOnlyOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule*, const Qn::UserAccessData& accessData, const Param&)
    {
        return hasSystemAccess(accessData)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct AllowForAllAccess
{
    template<typename Param>
    bool operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&) { return true; }
};

struct AllowForAllAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&) { return RemotePeerAccess::Allowed; }
};

bool resourceAccessHelper(
    QnCommonModule* commonModule,
    const Qn::UserAccessData& accessData,
    const QnUuid& resourceId,
    Qn::Permissions permissions)
{
    if (hasSystemAccess(accessData))
        return true;
    const auto& resPool = commonModule->resourcePool();
    QnResourcePtr target = resPool->getResourceById(resourceId);
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
    if (commonModule->resourceAccessManager()->hasGlobalPermission(userResource, Qn::GlobalAdminPermission))
        return true;

    if (permissions == Qn::ReadPermission
        && accessData.access == Qn::UserAccessData::Access::ReadAllResources)
            return true;

    bool result = commonModule->resourceAccessManager()->hasPermission(userResource, target, permissions);
    if (!result)
        NX_LOG(
            lit("%1 \n\tuser %2 with %3 permissions is asking for \
                \n\t%4 resource with %5 permissions and fails...")
                .arg(Q_FUNC_INFO)
                .arg(accessData.userId.toString())
                .arg((int)accessData.access)
                .arg(resourceId.toString())
                .arg(permissions), cl_logDEBUG1);

    return result;
}

struct ModifyResourceAccess
{
    ModifyResourceAccess(bool isRemove): isRemove(isRemove) {}

    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param& param)
    {
        if (hasSystemAccess(accessData))
            return true;
        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        QnResourcePtr target = resPool->getResourceById(param.id);

        bool result = false;
        if (isRemove)
            result = commonModule->resourceAccessManager()->hasPermission(userResource, target, Qn::RemovePermission);
        else if (!target)
            result = commonModule->resourceAccessManager()->canCreateResource(userResource, param);
        else
            result = commonModule->resourceAccessManager()->canModifyResource(userResource, target, param);

        if (!result)
            NX_LOG(lit("%1 resource access returned false. User resource: %3. Target resource: %4")
                .arg(isRemove ? "Remove" : "Modify")
                .arg(userResource ? userResource->getId().toString() : QString())
                .arg(target ? target->getId().toString() : QString()),
                cl_logWARNING);

        return result;
    }

    bool isRemove;
};

struct ModifyCameraDataAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiCameraData& param)
    {
        if (!hasSystemAccess(accessData))
        {
            if (!param.physicalId.isEmpty() && !param.id.isNull())
            {
                auto expectedId = ApiCameraData::physicalIdToId(param.physicalId);
                if (expectedId != param.id)
                    return false;
            }
        }

        return ModifyResourceAccess(/*isRemove*/ false)(commonModule, accessData, param);
    }
};


template<typename Param>
void applyColumnFilter(QnCommonModule*, const Qn::UserAccessData& /*accessData*/, Param& /*data*/) {}

void applyColumnFilter(QnCommonModule*, const Qn::UserAccessData& accessData, ApiMediaServerData& data)
{
    if (accessData != Qn::kSystemAccess)
        data.authKey.clear();
}

void applyColumnFilter(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ApiStorageData& data)
{
    if (!hasSystemAccess(accessData) && !commonModule->resourceAccessManager()->hasGlobalPermission(
            accessData,
            Qn::GlobalPermission::GlobalAdminPermission))
    {
        data.url = QnStorageResource::urlWithoutCredentials(data.url);
    }
}

struct ReadResourceAccess
{
    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, Param& param)
    {
        if (resourceAccessHelper(commonModule, accessData, param.id, Qn::ReadPermission))
        {
            applyColumnFilter(commonModule, accessData, param);
            return true;
        }
        return false;
    }
};

struct ReadResourceAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.id, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ReadResourceParamAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ApiResourceParamWithRefData& param)
    {
        if (resourceAccessHelper(commonModule, accessData, param.resourceId, Qn::ReadPermission))
        {
            operator()(commonModule, accessData, static_cast<ApiResourceParamData&>(param));
            return true;
        }
        return false;
    }

    bool operator()(QnCommonModule*, const Qn::UserAccessData& accessData, ApiResourceParamData& param)
    {
        namespace ahlp = access_helpers;
        ahlp::FilterFunctorListType filters = {
            static_cast<bool (*)(ahlp::Mode, const Qn::UserAccessData&, ahlp::KeyValueFilterType*)>(&ahlp::kvSystemOnlyFilter)
        };

        bool allowed;
        ahlp::KeyValueFilterType keyValue(param.name, &param.value);
        ahlp::applyValueFilters(ahlp::Mode::read, accessData, &keyValue, filters, &allowed);

        if (!allowed)
            return false;

        return true;
    }
};

struct ReadResourceParamAccessOut
{
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiResourceParamWithRefData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.resourceId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyResourceParamAccess
{
    ModifyResourceParamAccess(bool isRemove): isRemove(isRemove) {}

    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiResourceParamWithRefData& param)
    {
        if (hasSystemAccess(accessData))
            return true;

        if (isRemove)
        {
            const auto& resPool = commonModule->resourcePool();
            commonModule->resourceAccessManager()->hasPermission(resPool->getResourceById<QnUserResource>(accessData.userId),
                resPool->getResourceById(param.resourceId),
                Qn::RemovePermission);
        }

        Qn::Permissions permissions = Qn::SavePermission;
        if (param.name == Qn::USER_FULL_NAME)
            permissions |= Qn::WriteFullNamePermission;

        return resourceAccessHelper(commonModule, accessData, param.resourceId, permissions);
    }

    bool isRemove;
};

struct ReadFootageDataAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiServerFootageData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverGuid, Qn::ReadPermission);
    }
};

struct ReadFootageDataAccessOut
{
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiServerFootageData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverGuid, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyFootageDataAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiServerFootageData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverGuid, Qn::SavePermission);
    }
};

struct ReadCameraAttributesAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiCameraAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.cameraId, Qn::ReadPermission);
    }
};

struct ReadCameraAttributesAccessOut
{
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiCameraAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.cameraId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};
struct ModifyCameraAttributesAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiCameraAttributesData& param)
    {
        if (accessData == Qn::kSystemAccess)
            return true;

        if (!resourceAccessHelper(commonModule, accessData, param.cameraId, Qn::SavePermission))
        {
            qWarning() << "save ApiCameraAttributesData forbidden because no save permissions. id=" << param.cameraId;
            return false;
        }

        QnCamLicenseUsageHelper licenseUsageHelper(commonModule);
        QnVirtualCameraResourceList cameras;

        const auto& resPool = commonModule->resourcePool();
        auto camera = resPool->getResourceById(param.cameraId).dynamicCast<QnVirtualCameraResource>();
        if (!camera)
        {
            qWarning() << "save ApiCameraAttributesData forbidden because camera object is not exists. id=" << param.cameraId;
            return false;
        }

        // Check the license if and only if recording goes from 'off' to 'on' state
        const bool prevScheduleEnabled = !camera->isScheduleDisabled();
        if (prevScheduleEnabled != param.scheduleEnabled)
        {
            licenseUsageHelper.propose(camera, param.scheduleEnabled);
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                qWarning() << "save ApiCameraAttributesData forbidden because no license to enable recording. id=" << param.cameraId;
                return false;
            }
        }
        return true;
    }
};

struct ModifyCameraAttributesListAccess
{
    void operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ApiCameraAttributesDataList& param)
    {
        if (accessData == Qn::kSystemAccess)
            return;

        for (const auto& p : param)
            if (!resourceAccessHelper(commonModule, accessData, p.cameraId, Qn::SavePermission))
            {
                param = ApiCameraAttributesDataList();
                return;
            }

        QnCamLicenseUsageHelper licenseUsageHelper(commonModule);
        QnVirtualCameraResourceList cameras;

        const auto& resPool = commonModule->resourcePool();
        for (const auto& p : param)
        {
            auto camera = resPool->getResourceById(p.cameraId).dynamicCast<QnVirtualCameraResource>();
            if (!camera)
            {
                param = ApiCameraAttributesDataList();
                return;
            }
            cameras.push_back(camera);
            const bool prevScheduleEnabled = !camera->isScheduleDisabled();
            if (prevScheduleEnabled != p.scheduleEnabled)
                licenseUsageHelper.propose(camera, p.scheduleEnabled);
        }

        for (const auto& camera : cameras)
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                param = ApiCameraAttributesDataList();
                return;
            }
    }
};

struct ReadServerAttributesAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiMediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverId, Qn::ReadPermission);
    }
};

struct ReadServerAttributesAccessOut
{
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiMediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyServerAttributesAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiMediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverId, Qn::SavePermission);
    }
};

struct UserInputAccess
{
    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        if (hasSystemAccess(accessData))
            return true;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, Qn::GlobalUserInputPermission);
        return result;
    }
};

struct AdminOnlyAccess
{
    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        if (hasSystemAccess(accessData))
            return true;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, Qn::GlobalAdminPermission);
        return result;
    }
};

struct AdminOnlyAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        if (hasSystemAccess(accessData))
            return RemotePeerAccess::Allowed;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        RemotePeerAccess result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, Qn::GlobalAdminPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
        return result;
    }
};

struct RemoveUserRoleAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiIdData& param)
    {
        if (!AdminOnlyAccess()(commonModule, accessData, param))
        {
            qWarning() << "Removing user role is forbidden because the user has no admin access";
            return false;
        }

        const auto& resPool = commonModule->resourcePool();
        for (const auto& user: resPool->getResources<QnUserResource>())
        {
            if (user->userRoleId() == param.id)
            {
                qWarning() << "Removing user role is forbidden because the role is still used by the user " << user->getName();
                return false;
            }
        }

        return true;
    }
};

struct VideoWallControlAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ApiVideowallControlMessageData&)
    {
        if (hasSystemAccess(accessData))
            return true;
        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, Qn::GlobalControlVideoWallPermission);
        if (!result)
        {
            QString userName = userResource ? userResource->fullName() : lit("Unknown user");
            NX_LOG(lit("Access check for ApiVideoWallControlMessageData failed for user %1")
                .arg(userName),
                cl_logWARNING);
        }
        return result;
    }
};

struct LayoutTourAccess
{
    bool operator()(
        QnCommonModule* /*commonModule*/,
        const Qn::UserAccessData& accessData,
        const ApiLayoutTourData& tour)
    {
        return hasSystemAccess(accessData)
            || tour.parentId.isNull()
            || accessData.userId == tour.parentId;
    }
};

struct LayoutTourAccessById
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const ApiIdData& tourId)
    {
        const auto tour = commonModule->layoutTourManager()->tour(tourId.id);
        return !tour.isValid() //< Allow everyone to work with tours which are already deleted.
            || LayoutTourAccess()(commonModule, accessData, tour);
    }
};

struct InvalidFilterFunc
{
    template<typename ParamType>
    void operator()(QnCommonModule*, const Qn::UserAccessData&, ParamType&)
    {
        auto td = getTransactionDescriptorByParam<ParamType>();
        auto transactionName = td->getName();
        NX_ASSERT(0, lit("This transaction (%1) param type doesn't support filtering").arg(transactionName));
    }
};

template<typename SingleAccess>
struct AccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const Param& param)
    {
        return SingleAccess()(commonModule, accessData, param)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

template<typename SingleAccess >
struct FilterListByAccess
{
    template<typename ParamContainer>
    void operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, commonModule](typename ParamContainer::value_type &param)
        {
            return !SingleAccess()(commonModule, accessData, param);
        }), outList.end());
    }
};

template<>
struct FilterListByAccess<ModifyResourceAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    template<typename ParamContainer>
    void operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, this, commonModule](const typename ParamContainer::value_type &param)
        {
            return !ModifyResourceAccess(isRemove)(commonModule, accessData, param);
        }), outList.end());
    }

    bool isRemove;
};

template<>
struct FilterListByAccess<ModifyResourceParamAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    void operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ApiResourceParamWithRefDataList& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, commonModule, this](const ApiResourceParamWithRefData &param)
        {
            return !ModifyResourceParamAccess(isRemove)(commonModule, accessData, param);
        }), outList.end());
    }

    bool isRemove;
};

template<typename SingleAccess>
struct ModifyListAccess
{
    template<typename ParamContainer>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size())
        {
            NX_LOG(lit("Modify list access filtered out %1 entries from %2. Transaction: %3")
                .arg(paramContainer.size() - tmpContainer.size())
                .arg(paramContainer.size())
                .arg(getTransactionDescriptorByParam<ParamContainer>()->getName()),
                cl_logWARNING);
            return false;
        }
        return true;
    }
};

template<typename SingleAccess>
struct ReadListAccess
{
    template<typename ParamContainer>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        return tmpContainer.size() != paramContainer.size() && tmpContainer.empty();
    }
};

template<typename SingleAccess>
struct ReadListAccessOut
{
    template<typename ParamContainer>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size() && tmpContainer.empty())
            return RemotePeerAccess::Forbidden;
        if (tmpContainer.size() == paramContainer.size())
            return RemotePeerAccess::Allowed;
        return RemotePeerAccess::Partial;
    }
};

struct RegularTransactionType
{
    template<typename Param>
    ec2::TransactionType::Value operator()(QnCommonModule*, const Param&, detail::QnDbManager*)
    {
        return TransactionType::Regular;
    }
};

struct LocalTransactionType
{
    template<typename Param>
    ec2::TransactionType::Value operator()(QnCommonModule*, const Param&, detail::QnDbManager*)
    {
        return TransactionType::Local;
    }
};

ec2::TransactionType::Value getStatusTransactionTypeFromDb(const QnUuid& id, detail::QnDbManager* db)
{
    ApiMediaServerDataList serverDataList;
    ec2::ErrorCode errorCode = db->doQueryNoLock(id, serverDataList);

    if (errorCode != ErrorCode::ok || serverDataList.empty())
        return ec2::TransactionType::Unknown;

    return TransactionType::Local;
}

struct SetStatusTransactionType
{
    ec2::TransactionType::Value operator()(QnCommonModule* commonModule, const ApiResourceStatusData& params, detail::QnDbManager* db)
    {
        const auto& resPool = commonModule->resourcePool();
        QnResourcePtr resource = resPool->getResourceById<QnResource>(params.id);
        if (!resource)
            return getStatusTransactionTypeFromDb(params.id, db);
        if(resource.dynamicCast<QnMediaServerResource>())
            return TransactionType::Local;
        else
            return TransactionType::Regular;
    }
};

struct SaveUserTransactionType
{
    ec2::TransactionType::Value operator()(QnCommonModule*, const ApiUserData& params, detail::QnDbManager* /*db*/)
    {
        return params.isCloud ? TransactionType::Cloud : TransactionType::Regular;
    }
};

struct SetResourceParamTransactionType
{
    ec2::TransactionType::Value operator()(
		QnCommonModule*,
        const ApiResourceParamWithRefData& param,
        detail::QnDbManager* /*db*/)
    {
        if (param.resourceId == QnUserResource::kAdminGuid &&
            param.name == nx::settings_names::kNameSystemName)
        {
            // System rename MUST be propagated to Nx Cloud
            return TransactionType::Cloud;
        }

        return TransactionType::Regular;
    }
};

ec2::TransactionType::Value getRemoveUserTransactionTypeFromDb(
    const QnUuid& id,
    detail::QnDbManager* db)
{
    ApiUserDataList userDataList;
    ec2::ErrorCode errorCode = db->doQueryNoLock(id, userDataList);

    if (errorCode != ErrorCode::ok || userDataList.empty())
        return ec2::TransactionType::Unknown;

    return userDataList[0].isCloud ? TransactionType::Cloud : TransactionType::Regular;
}

struct RemoveUserTransactionType
{
    ec2::TransactionType::Value operator()(QnCommonModule* commonModule, const ApiIdData& params, detail::QnDbManager* db)
    {
        const auto& resPool = commonModule->resourcePool();
        auto user = resPool->getResourceById<QnUserResource>(params.id);
        if (!user)
            return getRemoveUserTransactionTypeFromDb(params.id, db);
        return user->isCloud() ? TransactionType::Cloud : TransactionType::Regular;
    }
};

#define TRANSACTION_DESCRIPTOR_APPLY( \
    _, \
    Key, \
    ParamType, \
    isPersistent, \
    isSystem, \
    getHashFunc, \
    triggerNotificationFunc, \
    checkSavePermissionFunc, \
    checkReadPermissionFunc, \
    filterBySavePermissionFunc, \
    filterByReadPermissionFunc, \
    checkRemotePeerAccessFunc, \
	getTransactionTypeFunc \
    ) \
    std::make_shared<TransactionDescriptor<ParamType>>( \
        ApiCommand::Key, \
        isPersistent, \
        isSystem, \
        #Key, \
        getHashFunc, \
        createDefaultSaveTransactionHelper(getHashFunc), \
        createDefaultSaveSerializedTransactionHelper(getHashFunc), \
        [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); },  \
        triggerNotificationFunc, \
        checkSavePermissionFunc, \
        checkReadPermissionFunc, \
        filterBySavePermissionFunc, \
        filterByReadPermissionFunc, \
        checkRemotePeerAccessFunc, \
        getTransactionTypeFunc),

DescriptorBaseContainer transactionDescriptors = {
    TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_DESCRIPTOR_APPLY)
};

#undef TRANSACTION_DESCRIPTOR_APPLY

} // namespace detail

detail::TransactionDescriptorBase *getTransactionDescriptorByValue(ApiCommand::Value v)
{
    auto it = detail::transactionDescriptors.get<0>().find(v);
    bool isEnd = it == detail::transactionDescriptors.get<0>().end();
    NX_ASSERT(!isEnd, "ApiCommand::Value not found");
    return isEnd ? nullptr : (*it).get();
}

detail::TransactionDescriptorBase *getTransactionDescriptorByName(const QString& s)
{
    auto it = detail::transactionDescriptors.get<1>().find(s);
    bool isEnd = it == detail::transactionDescriptors.get<1>().end();
    return isEnd ? nullptr : (*it).get();
}

} // namespace ec2
