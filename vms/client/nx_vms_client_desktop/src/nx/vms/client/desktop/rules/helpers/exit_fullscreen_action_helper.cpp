// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exit_fullscreen_action_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/models/business_rule_view_model.h>

namespace nx::vms::client::desktop {

namespace {

QIcon selectedIcon(const QIcon& base)
{
    return QIcon(core::Skin::maximumSizePixmap(base, QIcon::Selected, QIcon::Off, false));
}

QIcon invalidIcon()
{
    return qnSkin->icon("tree/buggy.svg");
}

QnLayoutResourceList getLayoutsInternal(const QnBusinessRuleViewModel* model)
{
    return model->resourcePool()->getResourcesByIds<QnLayoutResource>(
        model->actionResourcesRaw());
}

QSet<QnUuid> toIds(const QnResourceList& resources)
{
    return nx::utils::toQSet(resources.ids());
}

} // namespace

bool ExitFullscreenActionHelper::isLayoutValid(const QnBusinessRuleViewModel* model)
{
    return !getLayoutsInternal(model).empty();
}

QString ExitFullscreenActionHelper::layoutText(const QnBusinessRuleViewModel* model)
{
    const auto layouts = getLayoutsInternal(model);

    if (layouts.empty())
        return tr("Select layout...");

    if (layouts.size() == 1)
        return layouts.first()->getName();

    return tr("%n layouts", "", layouts.size());
}

QIcon ExitFullscreenActionHelper::layoutIcon(const QnBusinessRuleViewModel* model)
{
    const auto layouts = getLayoutsInternal(model);

    if (layouts.empty())
        return invalidIcon();

    if (layouts.size() > 1)
        return selectedIcon(qnResIconCache->icon(QnResourceIconCache::Layouts));

    return selectedIcon(qnResIconCache->icon(layouts.first()));
}

QString ExitFullscreenActionHelper::tableCellText(const QnBusinessRuleViewModel* model)
{
    return layoutText(model);
}

QIcon ExitFullscreenActionHelper::tableCellIcon(const QnBusinessRuleViewModel* model)
{
    return layoutIcon(model);
}

QnLayoutResourceList ExitFullscreenActionHelper::layouts(const QnBusinessRuleViewModel* model)
{
    return getLayoutsInternal(model);
}

QSet<QnUuid> ExitFullscreenActionHelper::layoutIds(const QnBusinessRuleViewModel* model)
{
    return toIds(getLayoutsInternal(model));
}

QSet<QnUuid> ExitFullscreenActionHelper::setLayouts(
    const QnBusinessRuleViewModel* model,
    const QnLayoutResourceList& layouts)
{
    return setLayoutIds(model, toIds(layouts));
}

QSet<QnUuid> ExitFullscreenActionHelper::setLayoutIds(
    const QnBusinessRuleViewModel* model,
    const QSet<QnUuid>& layoutIds)
{
    return layoutIds;
}

} // namespace nx::vms::client::desktop
