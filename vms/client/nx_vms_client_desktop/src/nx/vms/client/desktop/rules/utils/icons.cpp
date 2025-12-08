// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "icons.h"

#include <QtGui/QGuiApplication>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

namespace nx::vms::client::desktop::rules {

using nx::vms::client::core::ResourceIconCache;

namespace {

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kAttentionTheme =
    {{QnIcon::Normal, {.primary = "yellow"}}};
NX_DECLARE_COLORIZED_ICON(kAttentionIcon, "20x20/Solid/attention.svg", kAttentionTheme);

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kResourceIconTheme =
    {{QnIcon::Selected, {.primary = "light4"}}, {QnIcon::Disabled, {.primary = "light16"}}};

std::pair<QIcon, QIcon::Mode> getIcon(ResourceIconCache::Key key, QIcon::Mode mode)
{
    const auto pixmap = appContext()->resourceIconCache()->iconPixmap(
        key,
        core::kIconSize * qApp->devicePixelRatio(),
        kResourceIconTheme,
        mode);

    return {QIcon{pixmap}, mode};
}

std::pair<QIcon, QIcon::Mode> getIcon(const QnResourcePtr& resource, QIcon::Mode mode)
{
    return getIcon(appContext()->resourceIconCache()->key(resource), mode);
}

} // namespace

std::pair<QIcon, QIcon::Mode> attentionIcon()
{
    return {qnSkin->icon(kAttentionIcon), QIcon::Mode::Selected};
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::TargetLayoutField* field)
{
    const auto resource =
        context->resourcePool()->getResourceById<QnLayoutResource>(field->value());
    if (!resource)
        return getIcon(ResourceIconCache::Layout, QIcon::Mode::Disabled);

    return getIcon(
        resource->isShared() ? ResourceIconCache::SharedLayout : ResourceIconCache::Layout,
        QIcon::Mode::Selected);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::TargetDevicesField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(field->ids());
    const auto resourcesCount = resources.size() + static_cast<size_t>(field->useSource());

    if (resourcesCount == 0)
        return getIcon(ResourceIconCache::Camera, QIcon::Mode::Disabled);

    if (resourcesCount > 1)
        return getIcon(ResourceIconCache::Cameras, QIcon::Mode::Selected);

    if (field->useSource())
        return getIcon(ResourceIconCache::Camera, QIcon::Mode::Selected);

    return getIcon(resources.first(), QIcon::Mode::Selected);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::TargetLayoutsField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnLayoutResource>(field->value());

    if (resources.size() > 1)
        return getIcon(ResourceIconCache::Layouts, QIcon::Mode::Selected);

    if (resources.size() == 1)
        return getIcon(resources.first(), QIcon::Mode::Selected);

    return getIcon(ResourceIconCache::Layout, QIcon::Mode::Disabled);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::TargetServersField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnMediaServerResource>(field->ids());
    const auto resourcesCount = resources.size() + static_cast<size_t>(field->useSource());

    if (resourcesCount == 0)
        return getIcon(ResourceIconCache::Server, QIcon::Mode::Disabled);

    if (resourcesCount > 1)
        return getIcon(ResourceIconCache::Servers, QIcon::Mode::Selected);

    if (field->useSource())
        return getIcon(ResourceIconCache::Server, QIcon::Mode::Selected);

    return getIcon(resources.first(), QIcon::Mode::Selected);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::TargetDeviceField* field)
{
    if (field->useSource())
        return getIcon(ResourceIconCache::Camera, QIcon::Mode::Selected);

    auto cameraResource =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(field->id());
    if (cameraResource)
        return getIcon(cameraResource, QIcon::Mode::Selected);

    return getIcon(ResourceIconCache::Camera, QIcon::Mode::Disabled);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(
    SystemContext* context,
    vms::rules::TargetUsersField* field,
    int additionalCount,
    QValidator::State fieldValidity)
{
    if (fieldValidity == QValidator::State::Intermediate)
        return attentionIcon();

    if (field->acceptAll())
        return getIcon(ResourceIconCache::Users, QIcon::Mode::Selected);

    QnUserResourceList users;
    api::UserGroupDataList groups;
    common::getUsersAndGroups(context, field->ids(), users, groups);

    const auto usersCount = users.size() + additionalCount;

    if (usersCount == 0 && groups.empty())
        return getIcon(ResourceIconCache::User, QIcon::Mode::Disabled);

    if (usersCount == 1 && groups.empty())
        return getIcon(ResourceIconCache::User, QIcon::Mode::Selected);

    return getIcon(ResourceIconCache::Users, QIcon::Mode::Selected);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::SourceCameraField* field)
{
    if (field->acceptAll())
        return getIcon(ResourceIconCache::Cameras, QIcon::Mode::Selected);

    const auto resources =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(field->ids());

    if (resources.empty())
        return getIcon(ResourceIconCache::Camera, QIcon::Mode::Disabled);

    if (resources.size() == 1)
        return getIcon(resources.first(), QIcon::Mode::Selected);

    return getIcon(ResourceIconCache::Cameras, QIcon::Mode::Selected);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::SourceServerField* field)
{
    if (field->acceptAll())
        return getIcon(ResourceIconCache::Servers, QIcon::Mode::Selected);

    const auto resources =
        context->resourcePool()->getResourcesByIds<QnMediaServerResource>(field->ids());

    if (resources.size() > 1)
        return getIcon(ResourceIconCache::Servers, QIcon::Mode::Selected);

    return getIcon(ResourceIconCache::Server, resources.empty()
        ? QIcon::Mode::Normal
        : QIcon::Mode::Selected);
}

std::pair<QIcon, QIcon::Mode> selectButtonIcon(SystemContext* context, vms::rules::SourceUserField* field)
{
    if (field->acceptAll())
        return getIcon(ResourceIconCache::Users, QIcon::Mode::Selected);

    QnUserResourceList users;
    api::UserGroupDataList groups;
    common::getUsersAndGroups(context, field->ids(), users, groups);

    if (users.empty() && groups.empty())
        return getIcon(ResourceIconCache::User, QIcon::Mode::Disabled);

    if (users.size() == 1 && groups.empty())
        return getIcon(ResourceIconCache::User, QIcon::Mode::Selected);

    return getIcon(ResourceIconCache::Users, QIcon::Mode::Selected);
}

} // namespace nx::vms::client::desktop::rules
