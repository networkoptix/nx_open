#include "transaction_descriptor.h"
#include "transaction_message_bus.h"

#include <iostream>

#include <core/resource_management/user_access_data.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx_ec/data/api_tran_state_data.h>


namespace ec2 {
namespace detail {

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
    template<typename Param>
    QnUuid operator ()(const Param &param) { return QnTransactionLog::makeHash(param.id.toRfc4122()); }
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
            return notificationParams.mediaServerNotificationManager->triggerNotification(tran);
        case ApiCommand::removeResource:
            return notificationParams.resourceNotificationManager->triggerNotification(tran);
        case ApiCommand::removeCamera:
            return notificationParams.cameraNotificationManager->triggerNotification(tran);
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeStorage:
            return notificationParams.mediaServerNotificationManager->triggerNotification(tran);
        case ApiCommand::removeUser:
        case ApiCommand::removeUserGroup:
            return notificationParams.userNotificationManager->triggerNotification(tran);
        case ApiCommand::removeBusinessRule:
            return notificationParams.businessEventNotificationManager->triggerNotification(tran);
        case ApiCommand::removeLayout:
            return notificationParams.layoutNotificationManager->triggerNotification(tran);
        case ApiCommand::removeVideowall:
            return notificationParams.videowallNotificationManager->triggerNotification(tran);
        case ApiCommand::removeWebPage:
            return notificationParams.webPageNotificationManager->triggerNotification(tran);
        case ApiCommand::removeCameraUserAttributes:
            return notificationParams.cameraNotificationManager->triggerNotification(tran);
        case ApiCommand::forcePrimaryTimeServer:
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
    return QnTransactionLog::makeHash(params.cameraID.toRfc4122(), "camera_attributes");
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
    return QnTransactionLog::makeHash(params.serverID.toRfc4122(), "server_attributes");
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
        notificationParams.cameraNotificationManager->triggerNotification(tran);
    }
};

struct ResourceNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.resourceNotificationManager->triggerNotification(tran);
    }
};

struct MediaServerNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.mediaServerNotificationManager->triggerNotification(tran);
    }
};

struct UserNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.userNotificationManager->triggerNotification(tran);
    }
};

struct LayoutNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.layoutNotificationManager->triggerNotification(tran);
    }
};

struct VideowallNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.videowallNotificationManager->triggerNotification(tran);
    }
};

struct BusinessEventNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.businessEventNotificationManager->triggerNotification(tran);
    }
};

struct StoredFileNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.storedFileNotificationManager->triggerNotification(tran);
    }
};

struct LicenseNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.licenseNotificationManager->triggerNotification(tran);
    }
};

struct UpdateNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.updatesNotificationManager->triggerNotification(tran);
    }
};

struct DiscoveryNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.discoveryNotificationManager->triggerNotification(tran);
    }
};

struct WebPageNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.webPageNotificationManager->triggerNotification(tran);
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
            return notificationParams.mediaServerNotificationManager->triggerNotification(tran);
        case ApiCommand::removeResources:
            return notificationParams.resourceNotificationManager->triggerNotification(tran);
        default:
            NX_ASSERT(false);
    }
}

// Permission helpers
struct InvalidAccess
{
    template<typename Param>
    bool operator()(const Qn::UserAccessData&, const Param&)
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
    RemotePeerAccess operator()(const Qn::UserAccessData&, const Param&)
    {
        auto td = getTransactionDescriptorByParam<Param>();
        auto transactionName = td->getName();
        NX_ASSERT(0, lit("Invalid outgoing transaction access check (%1). We shouldn't be here.").arg(transactionName));
        return RemotePeerAccess::Forbidden;
    }
};

bool systemSuperAccess(const Qn::UserAccessData& accessData)
{
    return accessData.userId == Qn::kDefaultUserAccess.userId;
}

struct SystemSuperUserAccessOnly
{
    template<typename Param>
    bool operator()(const Qn::UserAccessData& accessData, const Param&)
    {
        return systemSuperAccess(accessData);
    }
};

struct SystemSuperUserAccessOnlyOut
{
    template<typename Param>
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const Param&)
    {
        return systemSuperAccess(accessData) ? RemotePeerAccess::Allowed : RemotePeerAccess::Forbidden;
    }
};

struct AllowForAllAccess
{
    template<typename Param>
    bool operator()(const Qn::UserAccessData&, const Param&) { return true; }
};

struct AllowForAllAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(const Qn::UserAccessData&, const Param&) { return RemotePeerAccess::Allowed; }
};

bool resourceAccessHelper(const Qn::UserAccessData& accessData, const QnUuid& resourceId, Qn::Permission permission)
{
    if (systemSuperAccess(accessData))
        return true;

    QnResourcePtr target = qnResPool->getResourceById(resourceId);
    auto userResource = qnResPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
    if (qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalAdminPermission))
        return true;

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

struct ModifyResourceAccess
{
    ModifyResourceAccess(bool isRemove): isRemove(isRemove) {}

    template<typename Param>
    bool operator()(const Qn::UserAccessData& accessData, const Param& param)
    {
        if (systemSuperAccess(accessData))
            return true;

        auto userResource = qnResPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        QnResourcePtr target = qnResPool->getResourceById(param.id);

        if (isRemove)
            return qnResourceAccessManager->hasPermission(userResource, target, Qn::RemovePermission);

        bool result = false;
        if (!target)
            result = qnResourceAccessManager->canCreateResource(userResource, param);
        else
            result = qnResourceAccessManager->canModifyResource(userResource, target, param);

        if (!result)
            NX_LOG(lit("Modify resource access returned false for transaction %1. User resource isNull: %2. Target resource isNull %3")
                .arg(getTransactionDescriptorByParam<Param>()->getName())
                .arg(!(bool)userResource)
                .arg(!(bool)target),
                cl_logDEBUG1);

        return result;
    }

    bool isRemove;
};

struct ReadResourceAccess
{
    template<typename Param>
    bool operator()(const Qn::UserAccessData& accessData, const Param& param)
    {
        return resourceAccessHelper(accessData, param.id, Qn::ReadPermission);
    }
};

struct ReadResourceAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const Param& param)
    {
        return resourceAccessHelper(accessData, param.id, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ReadResourceParamAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiResourceParamWithRefData& param)
    {
        return resourceAccessHelper(accessData, param.resourceId, Qn::ReadPermission);
    }
};

struct ReadResourceParamAccessOut
{
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const ApiResourceParamWithRefData& param)
    {
        return resourceAccessHelper(accessData, param.resourceId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyResourceParamAccess
{
    ModifyResourceParamAccess(bool isRemove): isRemove(isRemove) {}

    bool operator()(const Qn::UserAccessData& accessData, const ApiResourceParamWithRefData& param)
    {
        if (systemSuperAccess(accessData))
            return true;

        if (isRemove)
            return qnResourceAccessManager->hasPermission(qnResPool->getResourceById<QnUserResource>(accessData.userId),
                qnResPool->getResourceById(param.resourceId),
                Qn::RemovePermission);
        return resourceAccessHelper(accessData, param.resourceId, Qn::SavePermission);
    }

    bool isRemove;
};

struct ReadFootageDataAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiServerFootageData& param)
    {
        return resourceAccessHelper(accessData, param.serverGuid, Qn::ReadPermission);
    }
};

struct ReadFootageDataAccessOut
{
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const ApiServerFootageData& param)
    {
        return resourceAccessHelper(accessData, param.serverGuid, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyFootageDataAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiServerFootageData& param)
    {
        return resourceAccessHelper(accessData, param.serverGuid, Qn::SavePermission);
    }
};

struct ReadCameraAttributesAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiCameraAttributesData& param)
    {
        return resourceAccessHelper(accessData, param.cameraID, Qn::ReadPermission);
    }
};

struct ReadCameraAttributesAccessOut
{
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const ApiCameraAttributesData& param)
    {
        return resourceAccessHelper(accessData, param.cameraID, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};
struct ModifyCameraAttributesAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiCameraAttributesData& param)
    {
        return resourceAccessHelper(accessData, param.cameraID, Qn::SavePermission);
    }
};

struct ReadServerAttributesAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiMediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(accessData, param.serverID, Qn::ReadPermission);
    }
};

struct ReadServerAttributesAccessOut
{
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const ApiMediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(accessData, param.serverID, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyServerAttributesAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiMediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(accessData, param.serverID, Qn::SavePermission);
    }
};

struct AdminOnlyAccess
{
    template<typename Param>
    bool operator()(const Qn::UserAccessData& accessData, const Param&)
    {
        if (systemSuperAccess(accessData))
            return true;

        auto userResource = qnResPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalAdminPermission);
        return result;
    }
};

struct AdminOnlyAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const Param&)
    {
        if (systemSuperAccess(accessData))
            return RemotePeerAccess::Allowed;

        auto userResource = qnResPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        RemotePeerAccess result = qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalAdminPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
        return result;
    }
};

struct ControlVideowallAccess
{
    bool operator()(const Qn::UserAccessData& accessData, const ApiVideowallControlMessageData&)
    {
        if (systemSuperAccess(accessData))
            return true;
        auto userResource = qnResPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalControlVideoWallPermission);
        if (!result)
        {
            QString userName = userResource ? userResource->fullName() : lit("Unknown user");
            NX_LOG(lit("Access check for ApiVideoWallControlMessageData failed for user %1")
                .arg(userName),
                cl_logDEBUG1);
        }
        return result;
    }
};

struct InvalidFilterFunc
{
    template<typename ParamType>
    void operator()(const Qn::UserAccessData&, ParamType&)
    {
        auto td = getTransactionDescriptorByParam<ParamType>();
        auto transactionName = td->getName();
        NX_ASSERT(0, lit("This transaction (%1) param type doesn't support filtering").arg(transactionName));
    }
};

template<typename SingleAccess >
struct FilterListByAccess
{
    template<typename ParamContainer>
    void operator()(const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData](const typename ParamContainer::value_type &param)
        {
            return !SingleAccess()(accessData, param);
        }), outList.end());
    }
};

template<>
struct FilterListByAccess<ModifyResourceAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    template<typename ParamContainer>
    void operator()(const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, this](const typename ParamContainer::value_type &param)
        {
            return !ModifyResourceAccess(isRemove)(accessData, param);
        }), outList.end());
    }

    bool isRemove;
};

template<>
struct FilterListByAccess<ModifyResourceParamAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    void operator()(const Qn::UserAccessData& accessData, ApiResourceParamWithRefDataList& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, this](const ApiResourceParamWithRefData &param)
        {
            return !ModifyResourceParamAccess(isRemove)(accessData, param);
        }), outList.end());
    }

    bool isRemove;
};

template<typename SingleAccess>
struct ModifyListAccess
{
    template<typename ParamContainer>
    bool operator()(const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size())
        {
            NX_LOG(lit("Modify list access filtered out %1 entries from %2. Transaction: %3")
                .arg(paramContainer.size() - tmpContainer.size())
                .arg(paramContainer.size())
                .arg(getTransactionDescriptorByParam<ParamContainer>()->getName()),
                cl_logDEBUG1);
            return false;
        }
        return true;
    }
};

template<typename SingleAccess>
struct ReadListAccess
{
    template<typename ParamContainer>
    bool operator()(const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(accessData, tmpContainer);
        return tmpContainer.size() != paramContainer.size() && tmpContainer.empty();
    }
};

template<typename SingleAccess>
struct ReadListAccessOut
{
    template<typename ParamContainer>
    RemotePeerAccess operator()(const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size() && tmpContainer.empty())
            return RemotePeerAccess::Forbidden;
        if (tmpContainer.size() == paramContainer.size())
            return RemotePeerAccess::Allowed;
        return RemotePeerAccess::Partial;
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
    checkRemotePeerAccessFunc \
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
        checkRemotePeerAccessFunc),


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
    return (*it).get();
}

detail::TransactionDescriptorBase *getTransactionDescriptorByName(const QString &s)
{
    auto it = detail::transactionDescriptors.get<1>().find(s);
    bool isEnd = it == detail::transactionDescriptors.get<1>().end();
    NX_ASSERT(!isEnd, "ApiCommand name not found");
    return (*it).get();
}

} // namespace ec2
