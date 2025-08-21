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
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaxRequestsPerBatch = 500;

std::tuple<nx::network::rest::ErrorId, QString> extractError(
    const nx::vms::api::JsonRpcError& error)
{
    using JsonRpcError = nx::vms::api::JsonRpcError;
    if (error.code == JsonRpcError::applicationError && error.data)
    {
        nx::network::rest::Result result;
        if (nx::reflect::json::deserialize(
            nx::reflect::json::DeserializationContext{*error.data}, &result))
        {
            return {result.errorId, result.errorString};
        }
    }

    static const auto kRestApiError = nx::network::rest::ErrorId::serviceUnavailable;

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
            return {kRestApiError, UserGroupRequestChain::tr("Site Error")};
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

static QJsonObject paramsWithId(const nx::Uuid& id, QJsonObject params = {})
{
    params.insert("id", id.toSimpleString());
    return params;
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::RemoveUser& data,
    int reqId)
{
    return api::JsonRpcRequest::create(reqId, "rest.v4.users.delete", paramsWithId(data.id));
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::RemoveGroup& data,
    int reqId)
{
    return api::JsonRpcRequest::create(reqId, "rest.v4.userGroups.delete", paramsWithId(data.id));
}

static nx::vms::api::JsonRpcRequest toJsonRpcRequest(
    const UserGroupRequest::UpdateUser& data,
    int reqId)
{
    QJsonObject params{{{"isEnabled", data.enabled}}};
    if (data.enableDigest.has_value())
        params.insert("isHttpDigestEnabled", data.enableDigest.value());

    return api::JsonRpcRequest::create(
        reqId, "rest.v4.users.update", paramsWithId(data.id, params));
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

    return api::JsonRpcRequest::create(
        reqId, "rest.v4.userGroups.update", paramsWithId(mod.id, params));
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

    return api::JsonRpcRequest::create(
        reqId, "rest.v4.users.update", paramsWithId(mod.id, params));
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

    return api::JsonRpcRequest::create(reqId,
        "rest.v4.userGroups." + std::string(updateGroup.newGroup ? "create" : "update"),
        jsonObject);
}

} // namespace

class UserGroupRequestChain::Private
{
    UserGroupRequestChain* const q;

public:
    Private(UserGroupRequestChain* q): q(q) {}

    template <typename T>
    void gatherRequests(std::vector<api::JsonRpcRequest>& requests, size_t maxRequests);

    template<typename Result, typename Callback>
    void runRequests(std::vector<api::JsonRpcRequest> requests, Callback&& onSuccess);

    template <typename T, typename Result>
    std::function<void(const T&)> request(std::function<void(const Result&)> callback);

    template <typename T>
    std::function<void(const T&)> request(std::function<void()> callback);

    template <typename T>
    const T* getRequestData() const;

    void applyGroup(
        const nx::Uuid& groupId,
        const std::vector<nx::Uuid>& prev,
        const std::vector<nx::Uuid>& next);

    void applyUser(
        const nx::Uuid& userId,
        const std::vector<nx::Uuid>& prev,
        const std::vector<nx::Uuid>& next);

    void applyUser(
        const nx::Uuid& userId,
        bool enabled,
        std::optional<bool> enableDigest);

    void patchRequests(int startFrom, const nx::Uuid& oldId, const nx::Uuid& newId)
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

template<typename Result, typename Callback>
void UserGroupRequestChain::Private::runRequests(
    std::vector<api::JsonRpcRequest> requests, Callback&& onSuccess)
{
    if (!q->connectedServerApi())
    {
        // Use SessionExpired to suppress showing error message box - this is the same error as if
        // user cancels session refresh dialog.
        q->requestComplete(/*success*/ false, nx::network::rest::ErrorId::sessionExpired, {});
        return;
    }

    currentRequest = q->connectedServerApi()->jsonRpcBatchCall(
        q->tokenHelper(),
        std::move(requests),
        nx::utils::guarded(
            q,
            [this, onSuccess = std::move(onSuccess)](
                bool success, rest::Handle handle, const std::vector<api::JsonRpcResponse>& result)
            {
                if (NX_ASSERT(handle == currentRequest))
                    currentRequest = 0;

                nx::network::rest::ErrorId errorCode = nx::network::rest::ErrorId::ok;
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


                        const auto replyIdRollback = nx::utils::makeScopedRollback(replyId);

                        if (const auto* idPtr = std::get_if<nx::json_rpc::NumericId>(&response.id))
                        {
                            if (*idPtr >= 0 && (size_t) *idPtr < q->size())
                                replyId = *idPtr;
                        }

                        if constexpr (!std::is_void_v<Result>)
                        {
                            Result resultData;
                            if (!nx::reflect::json::deserialize(
                                nx::reflect::json::DeserializationContext{*response.result},
                                &resultData))
                            {
                                continue;
                            }

                            onSuccess(resultData);
                        }
                        else
                        {
                            onSuccess();
                        }
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
                        errorCode = nx::network::rest::ErrorId::serviceUnavailable;
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

            runRequests<Result>(std::move(requests),
                [callback = std::move(callback)](const Result& result)
                {
                    callback(result);
                });
        };
}

template <typename T>
std::function<void(const T&)> UserGroupRequestChain::Private::request(
    std::function<void()> callback)
{
    return
        [this, callback = std::move(callback)](const T& t)
        {
            std::vector<api::JsonRpcRequest> requests;

            gatherRequests<T>(requests, kMaxRequestsPerBatch);

            runRequests<void>(std::move(requests),
                [callback = std::move(callback)]
                {
                    callback();
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

        d->request<UserGroupRequest::RemoveUser>(
            [this]()
            {
                if (const auto data = d->getRequestData<UserGroupRequest::RemoveUser>())
                {
                    // Remove if not already removed.
                    const auto user =
                        systemContext()->resourcePool()->getResourceById<QnUserResource>(data->id);
                    if (user && !user->isChannelPartner())
                        systemContext()->resourcePool()->removeResource(user);
                }
            }),

        d->request<UserGroupRequest::RemoveGroup>(
            [this]()
            {
                if (const auto data = d->getRequestData<UserGroupRequest::RemoveGroup>())
                {
                    // Remove if not already removed.
                    userGroupManager()->remove(data->id);
                }
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
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& accessRights)
{
    const auto resourcePool = systemContext->resourcePool();

    const auto getKey = [](const auto& pair) { return pair.first; };

    const auto keys = nx::utils::rangeAdapter(
        boost::make_transform_iterator(accessRights.cbegin(), getKey),
        boost::make_transform_iterator(accessRights.cend(), getKey));

    const auto layouts = resourcePool->getResourcesByIds(keys).filtered<core::LayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return !layout->isFile()
                && !layout->isShared()
                && !layout->hasFlags(Qn::cross_system);
        });

    for (const auto& layout: layouts)
    {
        layout->setParentId(nx::Uuid());
        auto resourceSystemContext = SystemContext::fromResource(layout);
        if (!NX_ASSERT(resourceSystemContext))
            continue;

        layout->saveAsync();
    }
}

void UserGroupRequestChain::Private::applyGroup(
    const nx::Uuid& groupId,
    const std::vector<nx::Uuid>& prev,
    const std::vector<nx::Uuid>& next)
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
    const nx::Uuid& userId,
    const std::vector<nx::Uuid>& prev,
    const std::vector<nx::Uuid>& next)
{
    auto user = q->systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return;

    if (user->siteGroupIds() != prev)
        return;

    user->setSiteGroupIds(next);
}

void UserGroupRequestChain::Private::applyUser(
    const nx::Uuid& userId,
    bool enabled,
    std::optional<bool> enableDigest)
{
    auto user = q->systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return;

    user->setEnabled(enabled);

    if (!user->isTemporary() && enableDigest.has_value())
    {
        user->setPasswordAndGenerateHash(
            {},
            enableDigest.value()
                ? QnUserResource::DigestSupport::enable
                : QnUserResource::DigestSupport::disable);
    }
}

} // namespace nx::vms::client::desktop
