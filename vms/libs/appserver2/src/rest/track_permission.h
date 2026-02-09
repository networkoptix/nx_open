// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/handler.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::common { class SystemContext; }

class TrackPermission
{
public:
    TrackPermission(
        nx::vms::common::SystemContext* systemContext,
        nx::Uuid allAccessibleGroupId,
        nx::vms::api::AccessRight requiredAccess,
        nx::MoveOnlyFunc<void(
            const QString& id,
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
