// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "ec2_crud_handler.h"

class TrackPermission
{
public:
    TrackPermission(
        nx::vms::common::SystemContext* systemContext,
        nx::Uuid allAccessibleGroupId,
        nx::MoveOnlyFunc<void(const QString& id,
            nx::network::rest::Handler::NotifyType notifyType,
            std::optional<nx::Uuid> user,
            bool noEtag)> notify);

    virtual ~TrackPermission();
    nx::utils::Guard trackPermissions(const nx::Uuid& user);
    void addAccessible(const QString& id);
    void removeAccessible(const QString& id);

    template<typename T>
    std::set<QString> accessibleIds(const nx::Uuid& user, bool all) const;

protected:
    virtual std::set<QString> accessibleIds(const nx::Uuid& user, bool all) const = 0;

private:
    struct Private;
    friend struct Private;

private:
    std::unique_ptr<Private> d;
};

template<
    typename Derived,
    typename Filter,
    typename Model,
    typename DeleteInput,
    typename QueryProcessor,
    ec2::ApiCommand::Value DeleteCommand>
class TrackPermissionCrudHandler:
    public TrackPermission,
    public ec2::CrudHandler<Derived, Filter, Model, DeleteInput, QueryProcessor, DeleteCommand>
{
public:
    template<typename... Args>
    TrackPermissionCrudHandler(
        const nx::Uuid& allAccessibleGroupId,
        QueryProcessor* queryProcessor,
        Args&&... args)
        :
        TrackPermission(queryProcessor->systemContext(), allAccessibleGroupId,
            [this](auto&&... args)
            {
                TrackPermissionCrudHandler::CrudHandler::notify(std::forward<decltype(args)>(args)...);
            }),
        TrackPermissionCrudHandler::CrudHandler(queryProcessor, std::forward<Args>(args)...)
    {
    }

    nx::utils::Guard addSubscription(nx::network::rest::Request request,
        nx::network::rest::Handler::SubscriptionCallback callback)
    {
        const auto userId = request.userSession.access.userId;
        std::vector<nx::utils::Guard> guards;
        guards.push_back(TrackPermissionCrudHandler::CrudHandler::addSubscription(
            std::move(request), std::move(callback)));
        guards.push_back(trackPermissions(userId));
        return {[guards = std::move(guards)]() mutable { guards.clear(); }};
    }

    void notify(
        const QString& id,
        nx::network::rest::Handler::NotifyType notifyType,
        std::optional<nx::Uuid> user = {},
        bool noEtag = false)
    {
        switch (notifyType)
        {
            case nx::network::rest::Handler::NotifyType::update:
                addAccessible(id);
                break;
            case nx::network::rest::Handler::NotifyType::delete_:
                removeAccessible(id);
                break;
        }
        TrackPermissionCrudHandler::CrudHandler::notify(id, notifyType, std::move(user), noEtag);
    }
};
