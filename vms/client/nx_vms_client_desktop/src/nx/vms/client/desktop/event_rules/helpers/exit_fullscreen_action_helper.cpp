#include "exit_fullscreen_action_helper.h"

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/models/business_rule_view_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

namespace {

QIcon selectedIcon(const QIcon& base)
{
    return QIcon(QnSkin::maximumSizePixmap(base, QIcon::Selected, QIcon::Off, false));
}

QIcon invalidIcon()
{
    return qnSkin->icon(lit("tree/buggy.png"));
}

QnLayoutResourceList getLayoutsInternal(const QnBusinessRuleViewModel* model)
{
    return model->resourcePool()->getResourcesByIds<QnLayoutResource>(model->actionResourcesRaw());
}

QSet<QnUuid> toIds(const QnResourceList& resources)
{
    QSet<QnUuid> result;
    for (const auto& resource: resources)
        result.insert(resource->getId());
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

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
