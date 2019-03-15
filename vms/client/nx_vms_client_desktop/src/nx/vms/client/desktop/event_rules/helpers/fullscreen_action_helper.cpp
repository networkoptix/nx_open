#include "fullscreen_action_helper.h"

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

QnVirtualCameraResourcePtr getCameraInternal(const QnBusinessRuleViewModel* model)
{
    if (model->isUsingSourceCamera())
        return QnVirtualCameraResourcePtr();

    const auto resourceIds = model->actionResourcesRaw();
    const auto resourcePool = model->resourcePool();

    for (const auto& id: resourceIds)
    {
        if (auto camera = resourcePool->getResourceById<QnVirtualCameraResource>(id))
            return camera;
    }
    return QnVirtualCameraResourcePtr();
}

QnLayoutResourceList getLayoutsInternal(const QnBusinessRuleViewModel* model)
{
    return model->resourcePool()->getResourcesByIds<QnLayoutResource>(model->actionResourcesRaw());
}

QSet<QnUuid> toIds(const QnResourceList& resources)
{
    return resources.ids().toSet();
}

} // namespace

namespace nx::vms::client::desktop {

bool FullscreenActionHelper::isCameraValid(const QnBusinessRuleViewModel* model)
{
    if (model->isUsingSourceCamera())
        return true;

    return !getCameraInternal(model).isNull();
}

QString FullscreenActionHelper::cameraText(const QnBusinessRuleViewModel* model)
{
    if (model->isUsingSourceCamera())
        return tr("Source camera");

    const auto camera = getCameraInternal(model);
    if (camera)
        return camera->getName();

    return QnDeviceDependentStrings::getDefaultNameFromSet(
        model->resourcePool(),
        tr("Select device..."),
        tr("Select camera..."));
}

QIcon FullscreenActionHelper::cameraIcon(const QnBusinessRuleViewModel* model)
{
    if (model->isUsingSourceCamera())
        return selectedIcon(qnResIconCache->icon(QnResourceIconCache::Camera));

    const auto camera = getCameraInternal(model);
    if (camera)
    {
        if (cameraExistOnLayouts(model))
            return selectedIcon(qnResIconCache->icon(camera));
        else
            return selectedIcon(qnResIconCache->icon(
                QnResourceIconCache::Camera | QnResourceIconCache::Incompatible));
    }

    return invalidIcon();
}

bool FullscreenActionHelper::isLayoutValid(const QnBusinessRuleViewModel* model)
{
    return !getLayoutsInternal(model).empty();
}

QString FullscreenActionHelper::layoutText(const QnBusinessRuleViewModel* model)
{
    const auto layouts = getLayoutsInternal(model);

    if (layouts.empty())
        return tr("Select layout...");

    if (layouts.size() == 1)
        return layouts.first()->getName();

    return tr("%n layouts", "", layouts.size());
}

QIcon FullscreenActionHelper::layoutIcon(const QnBusinessRuleViewModel* model)
{
    const auto layouts = getLayoutsInternal(model);

    if (layouts.empty())
        return invalidIcon();

    if (layouts.size() > 1)
        return selectedIcon(qnResIconCache->icon(QnResourceIconCache::Layouts));

    return selectedIcon(qnResIconCache->icon(layouts.first()));
}

QString FullscreenActionHelper::tableCellText(const QnBusinessRuleViewModel* model)
{
    if (!isCameraValid(model))
        return cameraText(model);

    if (!isLayoutValid(model))
        return layoutText(model);

    return tr("%1 on %2", "Camera %1 on layout %2").arg(cameraText(model), layoutText(model));
}

QIcon FullscreenActionHelper::tableCellIcon(const QnBusinessRuleViewModel* model)
{
    if (!isLayoutValid(model) || !isCameraValid(model))
        return invalidIcon();

    return cameraIcon(model);
}

QnVirtualCameraResourcePtr FullscreenActionHelper::camera(const QnBusinessRuleViewModel* model)
{
    return getCameraInternal(model);
}

QSet<QnUuid> FullscreenActionHelper::cameraIds(const QnBusinessRuleViewModel* model)
{
    QSet<QnUuid> result;
    if (const auto camera = getCameraInternal(model))
        result.insert(camera->getId());
    return result;
}

QSet<QnUuid> FullscreenActionHelper::setCamera(
    const QnBusinessRuleViewModel* model,
    const QnVirtualCameraResourcePtr& camera)
{
    auto result = layoutIds(model);
    if (camera)
        result.insert(camera->getId());
    return result;
}

QSet<QnUuid> FullscreenActionHelper::setCameraIds(
    const QnBusinessRuleViewModel* model,
    const QSet<QnUuid>& cameraIds)
{
    auto result = layoutIds(model);
    if (!cameraIds.empty())
        result.insert(*cameraIds.cbegin());
    return result;
}

QnLayoutResourceList FullscreenActionHelper::layouts(const QnBusinessRuleViewModel* model)
{
    return getLayoutsInternal(model);
}

QSet<QnUuid> FullscreenActionHelper::layoutIds(const QnBusinessRuleViewModel* model)
{
    return toIds(getLayoutsInternal(model));
}

QSet<QnUuid> FullscreenActionHelper::setLayouts(
    const QnBusinessRuleViewModel* model,
    const QnLayoutResourceList& layouts)
{
    return setLayoutIds(model, toIds(layouts));
}

QSet<QnUuid> FullscreenActionHelper::setLayoutIds(
    const QnBusinessRuleViewModel* model,
    const QSet<QnUuid>& layoutIds)
{
    auto result = layoutIds;
    if (const auto camera = getCameraInternal(model))
        result.insert(camera->getId());
    return result;
}

bool FullscreenActionHelper::cameraExistOnLayouts(const QnBusinessRuleViewModel* model)
{
    const auto camera = getCameraInternal(model);
    if (!camera)
        return true;

    const auto layouts = getLayoutsInternal(model);
    return std::all_of(layouts.cbegin(), layouts.cend(),
        [id = camera->getId()](const auto& layout)
        {
            return layout->layoutResourceIds().contains(id);
        });
}

} // namespace nx::vms::client::desktop
