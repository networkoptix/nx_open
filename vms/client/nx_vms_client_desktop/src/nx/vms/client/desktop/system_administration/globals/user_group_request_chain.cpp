// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_request_chain.h"

#include <boost/iterator/transform_iterator.hpp>

#include <api/server_rest_connection.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/compound_visitor.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scoped_rollback.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaxRequestsPerBatch = 500;

std::tuple<nx::network::rest::Result::Error, QString> extractError(
    const nx::vms::api::JsonRpcError& error)
{
    using JsonRpcError = nx::vms::api::JsonRpcError;
    if (error.code == JsonRpcError::applicationError && error.data)
    {
        nx::network::rest::Result result;
        if (QJson::deserialize(*error.data, &result))
            return {result.error, result.errorString};
    }

    static const auto kRestApiError = nx::network::rest::Result::ServiceUnavailable;

    if (!error.message.empty())
        return {kRestApiError, QString::fromStdString(error.message)};

    switch (error.code)
    {
        case JsonRpcError::parseError:
            return {kRestApiError, UserGroupRequestChain::tr("Invalid JSON")};
        case JsonRpcError::encodingError:
            return {kRestApiError, UserGroupRequestChain::tr("Invalid encoding")};
        case JsonRpcError::charsetError:
            return {kRestApiError, UserGroupRequestChain::tr("Invalid encoding charset")};

        case JsonRpcError::invalidRequest:
            return {kRestApiError, UserGroupRequestChain::tr("Invalid request")};
        case JsonRpcError::methodNotFound:
            return {kRestApiError, UserGroupRequestChain::tr("Method not found")};
        case JsonRpcError::invalidParams:
            return {kRestApiError, UserGroupRequestChain::tr("Invalid parameters")};
        case JsonRpcError::internalError:
            return {kRestApiError, UserGroupRequestChain::tr("Internal error")};

        case JsonRpcError::applicationError:
            return {kRestApiError, UserGroupRequestChain::tr("Application Error")};
        case JsonRpcError::systemError:
            return {kRestApiError, UserGroupRequestChain::tr("System Error")};
        case JsonRpcError::transportError:
            return {kRestApiError, UserGroupRequestChain::tr("Transport Error")};
    }

    if (error.code >= JsonRpcError::serverErrorBegin && error.code <= JsonRpcError::serverErrorEnd)
        return {kRestApiError, UserGroupRequestChain::tr("Server error code %1").arg(error.code)};

    if (error.code >= JsonRpcError::reservedErrorBegin && error.code <= JsonRpcError::reservedErrorEnd)
        return {kRestApiError, UserGroupRequestChain::tr("Reserved error code %1").arg(error.code)};

    return {kRestApiError, UserGroupRequestChain::tr("Unknown error code %1").arg(error.code)};
}

template <auto member, class T>
QJsonValue getJsonValue(const T& t)
{
    QnJsonContext context;
    context.setSerializeMapToObject(true);
    context.setChronoSerializedAsDouble(true);
    QJsonValue v;
    QJson::serialize(&context, t.*member, &v);
    return v;
}

#define JsonField(value, member) \
    QPair<QString, QJsonValue>( \
        #member, \
        getJsonValue<&std::decay_t<decltype(value)>::member>(value))

static QJsonObject paramsWithId(const QnUuid& id, QJsonObject params = {})
{
    params.insert("id", id.toString());
    return params;
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::RemoveUser& data,
    int reqId)
{
    return api::JsonRpcRequest{
        .method = "rest.v3.users.delete",
        .params = paramsWithId(data.id),
        .id = reqId
    };
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::RemoveGroup& data,
    int reqId)
{
    return api::JsonRpcRequest{
        .method = "rest.v3.userGroups.delete",
        .params = paramsWithId(data.id),
        .id = reqId
    };
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::UpdateUser& data,
    int reqId)
{
    nx::vms::api::UserModelV3 user;

    user.isEnabled = data.enabled;
    user.isHttpDigestEnabled = data.enableDigest;

    const QJsonObject params{
        JsonField(user, isEnabled),
        JsonField(user, isHttpDigestEnabled)
    };

    return api::JsonRpcRequest{
        .method = "rest.v3.users.update",
        .params = paramsWithId(data.id, params),
        .id = reqId
    };
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::ModifyGroupParents& mod,
    int reqId)
{
    nx::vms::api::UserGroupModel group;

    group.parentGroupIds = mod.newParents;

    const QJsonObject params {
        JsonField(group, parentGroupIds)
    };

    return api::JsonRpcRequest{
        .method = "rest.v3.userGroups.update",
        .params = paramsWithId(mod.id, params),
        .id = reqId
    };
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::ModifyUserParents& mod,
    int reqId)
{
    nx::vms::api::UserModelV3 user;

    user.groupIds = mod.newParents;

    const QJsonObject params{
        JsonField(user, groupIds)
    };

    return api::JsonRpcRequest{
        .method = "rest.v3.users.update",
        .params = paramsWithId(mod.id, params),
        .id = reqId
    };
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::AddOrUpdateGroup& updateGroup,
    int reqId)
{
    const nx::vms::api::UserGroupModel group = updateGroup.groupData;

    QnJsonContext context;
    context.setSerializeMapToObject(true);
    context.setChronoSerializedAsDouble(true);
    QJsonValue jsonValue;
    QJson::serialize(&context, group, &jsonValue);

    // Method 'create' does not accept 'id' field.
    QJsonObject jsonObject = jsonValue.toObject();
    if (updateGroup.newGroup)
        jsonObject.remove(JsonField(group, id).first);

    return api::JsonRpcRequest{
        .method = "rest.v3.userGroups." + std::string(updateGroup.newGroup ? "create" : "update"),
        .params = jsonObject,
        .id = reqId
    };
}

} // namespace

class UserGroupRequestChain::Private
{
    UserGroupRequestChain* const q;

public:
    Private(UserGroupRequestChain* q): q(q) {}

    template <typename T>
    void gatherRequests(std::vector<api::JsonRpcRequest>& requests, size_t maxRequests);

    template <typename T>
    void runRequests(
        const std::vector<api::JsonRpcRequest>& requests,
        std::function<void(const T&)> onSuccess);

    template <typename T, typename Result>
    std::function<void(const T&)> request(std::function<void(const Result&)> callback);

    template <typename T>
    const T* getRequestData() const;

    nx::vms::api::UserGroupModel fromGroupId(const QnUuid& groupId);
    nx::vms::api::UserModelV3 fromUserId(const QnUuid& userId);

    void applyGroup(
        const QnUuid& groupId,
        const std::vector<QnUuid>& prev,
        const std::vector<QnUuid>& next);

    void applyUser(
        const QnUuid& userId,
        const std::vector<QnUuid>& prev,
        const std::vector<QnUuid>& next);

    void applyUser(
        const QnUuid& userId,
        bool enabled,
        bool enableDigest);

    void patchRequests(int startFrom, const QnUuid& oldId, const QnUuid& newId)
    {
        for (int i = startFrom; (size_t)i < q->size(); ++i)
        {
            if (auto mod = get_if<UserGroupRequest::ModifyUserParents>(&(*q)[i]))
                std::replace(mod->newParents.begin(), mod->newParents.end(), oldId, newId);
            else if (auto mod = get_if<UserGroupRequest::ModifyGroupParents>(&(*q)[i]))
                std::replace(mod->newParents.begin(), mod->newParents.end(), oldId, newId);
        }
    }

public:
    nx::vms::common::SessionTokenHelperPtr tokenHelper;
    rest::Handle currentRequest{};
    int replyId = -1;
};

UserGroupRequestChain::UserGroupRequestChain(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    base_type(systemContext),
    d(new Private(this))
{
}

UserGroupRequestChain::~UserGroupRequestChain()
{
    if (d->currentRequest != 0)
        systemContext()->connectedServerApi()->cancelRequest(d->currentRequest);
}

bool UserGroupRequestChain::isRunning() const
{
    return d->currentRequest != 0;
}

void UserGroupRequestChain::setTokenHelper(nx::vms::common::SessionTokenHelperPtr tokenHelper)
{
    d->tokenHelper = tokenHelper;
}

nx::vms::common::SessionTokenHelperPtr UserGroupRequestChain::tokenHelper() const
{
    return d->tokenHelper
        ? d->tokenHelper
        : systemContext()->restApiHelper()->getSessionTokenHelper();
}

template <typename T>
const T* UserGroupRequestChain::Private::getRequestData() const
{
    if (replyId < 0 || (size_t) replyId >= q->size())
        return nullptr;

    const auto data = std::get_if<T>(&(*q)[replyId]);
    if (!data)
    {
        NX_ASSERT(this, "Expected request type %1", typeid(T));
        return nullptr;
    }

    return data;
}

template <typename T>
void UserGroupRequestChain::Private::gatherRequests(
    std::vector<api::JsonRpcRequest>& requests,
    size_t maxRequests)
{
    // Gather requests of the same type up to maxRequests.
    while (q->hasNext() && requests.size() < maxRequests)
    {
        const auto userData = std::get_if<T>(&q->peekNext());
        if (!userData)
            break;

        requests.emplace_back(toJsonRpcRequest(*userData, q->nextIndex()));
        q->advance();
    }
}

template <typename Result>
void UserGroupRequestChain::Private::runRequests(
    const std::vector<api::JsonRpcRequest>& requests,
    std::function<void(const Result&)> onSuccess)
{
    if (!q->connectedServerApi())
    {
        // Use SessionExpired to suppress showing error message box - this is the same error as if
        // user cancels session refresh dialog.
        q->requestComplete(/*success*/ false, nx::network::rest::Result::SessionExpired, {});
        return;
    }

    currentRequest = q->connectedServerApi()->jsonRpcBatchCall(
        q->tokenHelper(),
        requests,
        nx::utils::guarded(
            q,
            [this, onSuccess = std::move(onSuccess)](
                bool success, rest::Handle handle, const std::vector<api::JsonRpcResponse>& result)
            {
                if (NX_ASSERT(handle == currentRequest))
                    currentRequest = 0;

                nx::network::rest::Result::Error errorCode = nx::network::rest::Result::NoError;
                QString errorString;

                if (success)
                {
                    for (const auto& response: result)
                    {
                        if (response.error)
                        {
                            success = false;
                            std::tie(errorCode, errorString) = extractError(*response.error);
                            break;
                        }

                        if (!response.result)
                            continue;

                        Result resultData;
                        if (!QJson::deserialize(*response.result, &resultData))
                            continue;

                        const auto replyIdRollback = nx::utils::makeScopedRollback(replyId);

                        if (const int* idPtr = std::get_if<int>(&response.id))
                        {
                            if (*idPtr >= 0 && (size_t) *idPtr < q->size())
                                replyId = *idPtr;
                        }

                        onSuccess(resultData);
                    }
                }
                else
                {
                    if (!result.empty() && result.front().error)
                    {
                        std::tie(errorCode, errorString) = extractError(*result.front().error);
                    }
                    else
                    {
                        errorString = tr("Connection failure");
                        errorCode = nx::network::rest::Result::ServiceUnavailable;
                    }
                }

                q->requestComplete(success, errorCode, errorString);
            }),
        q->thread());
}

template <typename T, typename Result>
std::function<void(const T&)> UserGroupRequestChain::Private::request(
    std::function<void(const Result&)> callback)
{
    return
        [this, callback = std::move(callback)](const T& t)
        {
            std::vector<api::JsonRpcRequest> requests;

            gatherRequests<T>(requests, kMaxRequestsPerBatch);

            runRequests<Result>(requests,
                [callback = std::move(callback)](const Result& result)
                {
                    callback(result);
                });
        };
}

void UserGroupRequestChain::makeRequest()
{
    NX_ASSERT(d->currentRequest == 0);

    std::visit(nx::utils::CompoundVisitor{
        d->request<UserGroupRequest::ModifyUserParents, nx::vms::api::UserModelV3>(
            [this](const nx::vms::api::UserModelV3& user)
            {
                if (const auto data = d->getRequestData<UserGroupRequest::ModifyUserParents>())
                {
                    if (NX_ASSERT(data->id == user.id))
                        d->applyUser(data->id, data->prevParents, user.groupIds);
                }
            }),

        d->request<UserGroupRequest::ModifyGroupParents, nx::vms::api::UserGroupModel>(
            [this](const nx::vms::api::UserGroupModel& group)
            {
                if (const auto data = d->getRequestData<UserGroupRequest::ModifyGroupParents>())
                {
                    if (NX_ASSERT(data->id == group.id))
                        d->applyGroup(data->id, data->prevParents, group.parentGroupIds);
                }
            }),

        d->request<UserGroupRequest::RemoveUser, nx::vms::api::UserModelV3>(
            [this](const nx::vms::api::UserModelV3& user)
            {
                // Remove if not already removed.
                if (auto resource = systemContext()->resourcePool()->getResourceById(user.id))
                    systemContext()->resourcePool()->removeResource(resource);
            }),

        d->request<UserGroupRequest::RemoveGroup, nx::vms::api::UserGroupModel>(
            [this](const nx::vms::api::UserGroupModel& group)
            {
                // Remove if not already removed.
                userGroupManager()->remove(group.id);
            }),

        d->request<UserGroupRequest::UpdateUser, nx::vms::api::UserModelV3>(
            [this](const nx::vms::api::UserModelV3& user)
            {
                if (const auto data = d->getRequestData<UserGroupRequest::UpdateUser>())
                {
                    if (NX_ASSERT(data->id == user.id))
                        d->applyUser(data->id, data->enabled, data->enableDigest);
                }
            }),

        d->request<UserGroupRequest::AddOrUpdateGroup, nx::vms::api::UserGroupModel>(
            [this](const nx::vms::api::UserGroupModel& userGroup)
            {
                auto group =
                    systemContext()->userGroupManager()->find(
                        userGroup.id).value_or(api::UserGroupData{});

                // Update group locally ahead of receiving update from the server
                // to avoid UI blinking.
                group.id = userGroup.id;
                group.name = userGroup.name;
                group.description = userGroup.description;
                group.parentGroupIds = userGroup.parentGroupIds;
                group.permissions = userGroup.permissions;
                userGroupManager()->addOrUpdate(group);

                updateLayoutSharing(
                    systemContext(), userGroup.resourceAccessRights);

                // Update access rights locally.
                systemContext()->accessRightsManager()->setOwnResourceAccessMap(userGroup.id,
                    {userGroup.resourceAccessRights.begin(), userGroup.resourceAccessRights.end()});

                // A workaround - replace temporary group id with created group id.
                if (const auto data = d->getRequestData<UserGroupRequest::AddOrUpdateGroup>();
                    data && data->newGroup && d->replyId >= 0)
                {
                    d->patchRequests(d->replyId + 1, data->groupData.id, userGroup.id);
                }
            }),
        },
        peekNext());
}

void UserGroupRequestChain::updateLayoutSharing(
    nx::vms::client::desktop::SystemContext* systemContext,
    const std::map<QnUuid, nx::vms::api::AccessRights>& accessRights)
{
    const auto resourcePool = systemContext->resourcePool();

    const auto getKey = [](const auto& pair) { return pair.first; };

    const auto keys = nx::utils::rangeAdapter(
        boost::make_transform_iterator(accessRights.cbegin(), getKey),
        boost::make_transform_iterator(accessRights.cend(), getKey));

    const auto layouts = resourcePool->getResourcesByIds(keys).filtered<LayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return !layout->isFile()
                && !layout->isShared()
                && !layout->hasFlags(Qn::cross_system);
        });

    for (const auto& layout: layouts)
    {
        layout->setParentId(QnUuid());
        auto resourceSystemContext = SystemContext::fromResource(layout);
        if (!NX_ASSERT(resourceSystemContext))
            continue;

        resourceSystemContext->layoutSnapshotManager()->save(layout);
    }
}

nx::vms::api::UserGroupModel UserGroupRequestChain::Private::fromGroupId(const QnUuid& groupId)
{
    const auto userGroup = q->systemContext()->userGroupManager()->find(groupId);
    if (!userGroup)
        return {};

    nx::vms::api::UserGroupModel groupData;

    // Fill in all non-optional fields.
    groupData.id = userGroup->id;
    groupData.name = userGroup->name;
    groupData.description = userGroup->description;
    groupData.type = userGroup->type;
    groupData.permissions = userGroup->permissions;
    groupData.parentGroupIds = userGroup->parentGroupIds;
    const auto ownResourceAccessMap =
        q->systemContext()->accessRightsManager()->ownResourceAccessMap(
            userGroup->id).asKeyValueRange();
    groupData.resourceAccessRights = {ownResourceAccessMap.begin(), ownResourceAccessMap.end()};
    return groupData;
}

nx::vms::api::UserModelV3 UserGroupRequestChain::Private::fromUserId(const QnUuid& userId)
{
    const auto user = q->systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return {};

    nx::vms::api::UserModelV3 userData;

    // Fill in all non-optional fields.
    userData.id = user->getId();
    userData.name = user->getName();
    userData.email = user->getEmail();
    userData.type = user->userType();
    userData.fullName = user->fullName();
    userData.permissions = user->getRawPermissions();
    userData.isEnabled = user->isEnabled();
    userData.isHttpDigestEnabled =
        user->getDigest() != nx::vms::api::UserData::kHttpIsDisabledStub;

    userData.groupIds = user->groupIds();

    const auto ownResourceAccessMap =
        q->systemContext()->accessRightsManager()->ownResourceAccessMap(
            user->getId()).asKeyValueRange();
    userData.resourceAccessRights = {ownResourceAccessMap.begin(), ownResourceAccessMap.end()};

    return userData;
}

void UserGroupRequestChain::Private::applyGroup(
    const QnUuid& groupId,
    const std::vector<QnUuid>& prev,
    const std::vector<QnUuid>& next)
{
    auto userGroup = q->systemContext()->userGroupManager()->find(groupId);
    if (!userGroup)
        return;

    if (userGroup->parentGroupIds != prev)
        return; //< Already updated.

    userGroup->parentGroupIds = next;
    q->systemContext()->userGroupManager()->addOrUpdate(*userGroup);
}

void UserGroupRequestChain::Private::applyUser(
    const QnUuid& userId,
    const std::vector<QnUuid>& prev,
    const std::vector<QnUuid>& next)
{
    auto user = q->systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return;

    if (user->groupIds() != prev)
        return;

    user->setGroupIds(next);
}

void UserGroupRequestChain::Private::applyUser(
    const QnUuid& userId,
    bool enabled,
    bool enableDigest)
{
    auto user = q->systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return;

    // TODO: enableDigest shoud be optional in API structure.

    user->setEnabled(enabled);

    if (!user->isTemporary())
    {
        user->setPasswordAndGenerateHash(
            {},
            enableDigest
                ? QnUserResource::DigestSupport::enable
                : QnUserResource::DigestSupport::disable);
    }
}

} // namespace nx::vms::client::desktop
