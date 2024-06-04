// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "icons.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

namespace nx::vms::client::desktop::rules {

namespace {

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kAttentionTheme =
    {{QnIcon::Normal, {.primary = "yellow_d"}}};
NX_DECLARE_COLORIZED_ICON(kAttentionIcon, "20x20/Solid/attention.svg", kAttentionTheme);

template<class T>
QIcon getIcon(const T& value)
{
    return core::Skin::maximumSizePixmap(
        qnResIconCache->icon(value),
        QIcon::Selected,
        QIcon::Off,
        /*correctDevicePixelRatio*/ false);
}

} // namespace

QIcon attentionIcon()
{
    return qnSkin->icon(kAttentionIcon);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::LayoutField* field)
{
    if (const auto resource =
        context->resourcePool()->getResourceById<QnLayoutResource>(field->value()))
    {
        if (resource->isShared())
            return getIcon(QnResourceIconCache::SharedLayout);

        if (resource->locked())
            return getIcon(QnResourceIconCache::Layout | QnResourceIconCache::Locked);
    }

    return getIcon(QnResourceIconCache::Layout);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetDeviceField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(field->ids());
    const auto resourcesCount = resources.size() + static_cast<size_t>(field->useSource());

    if (resourcesCount > 1)
        return getIcon(QnResourceIconCache::Cameras);

    if (resources.size() == 1)
        return getIcon(resources.first());

    return getIcon(QnResourceIconCache::Camera);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetLayoutField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnLayoutResource>(field->value());

    if (resources.size() > 1)
        return getIcon(QnResourceIconCache::Layouts);

    if (resources.size() == 1)
        return getIcon(resources.first());

    return getIcon(QnResourceIconCache::Layout);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetServerField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnMediaServerResource>(field->ids());
    const auto resourcesCount = resources.size() + static_cast<size_t>(field->useSource());

    if (resourcesCount > 1)
        return getIcon(QnResourceIconCache::Servers);

    if (resources.size() == 1)
        return getIcon(resources.first());

    return getIcon(QnResourceIconCache::Server);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetSingleDeviceField* field)
{
    if (field->useSource() || field->id().isNull())
        return getIcon(QnResourceIconCache::Camera);

    return getIcon(context->resourcePool()->getResourceById<QnVirtualCameraResource>(field->id()));
}

QIcon selectButtonIcon(
    SystemContext* context, vms::rules::TargetUserField* field, int additionalCount)
{
    if (field->acceptAll())
        return getIcon(QnResourceIconCache::Users);

    QnUserResourceList users;
    api::UserGroupDataList groups;
    common::getUsersAndGroups(context, field->ids(), users, groups);

    const auto usersCount = users.size() + additionalCount;
    if (usersCount <= 1 && groups.empty())
        return getIcon(QnResourceIconCache::User);

    return getIcon(QnResourceIconCache::Users);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::SourceCameraField* field)
{
    if (field->acceptAll())
        return getIcon(QnResourceIconCache::Cameras);

    const auto resources =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(field->ids());

    if (resources.empty())
        return getIcon(QnResourceIconCache::Camera);

    if (resources.size() == 1)
        return getIcon(resources.first());

    return getIcon(QnResourceIconCache::Cameras);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::SourceServerField* field)
{
    if (field->acceptAll())
        return getIcon(QnResourceIconCache::Servers);

    const auto resources =
        context->resourcePool()->getResourcesByIds<QnMediaServerResource>(field->ids());

    if (resources.size() <= 1)
        return getIcon(QnResourceIconCache::Server);

    return getIcon(QnResourceIconCache::Servers);
}

QIcon selectButtonIcon(SystemContext* context, vms::rules::SourceUserField* field)
{
    if (field->acceptAll())
        return getIcon(QnResourceIconCache::Users);

    QnUserResourceList users;
    api::UserGroupDataList groups;
    common::getUsersAndGroups(context, field->ids(), users, groups);

    if (users.size() <= 1 && groups.empty())
        return getIcon(QnResourceIconCache::User);

    return getIcon(QnResourceIconCache::Users);
}

} // namespace nx::vms::client::desktop::rules
