// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fullscreen_action_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
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

QnVirtualCameraResourcePtr getCameraInternal(const QnBusinessRuleViewModel* model)
{
    if (model->isActionUsingSourceCamera())
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
    return model->resourcePool()->getResourcesByIds<QnLayoutResource>(
        model->actionResourcesRaw());
}

QSet<nx::Uuid> toIds(const QnResourceList& resources)
{
    return nx::utils::toQSet(resources.ids());
}

} // namespace

bool FullscreenActionHelper::isCameraValid(const QnBusinessRuleViewModel* model)
{
    if (model->isActionUsingSourceCamera())
        return true;

    return !getCameraInternal(model).isNull();
}

QString FullscreenActionHelper::cameraText(const QnBusinessRuleViewModel* model)
{
    if (model->isActionUsingSourceCamera())
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
    if (model->isActionUsingSourceCamera())
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

QSet<nx::Uuid> FullscreenActionHelper::cameraIds(const QnBusinessRuleViewModel* model)
{
    QSet<nx::Uuid> result;
    if (const auto camera = getCameraInternal(model))
        result.insert(camera->getId());
    return result;
}

QSet<nx::Uuid> FullscreenActionHelper::setCamera(
    const QnBusinessRuleViewModel* model,
    const QnVirtualCameraResourcePtr& camera)
{
    auto result = layoutIds(model);
    if (camera)
        result.insert(camera->getId());
    return result;
}

QSet<nx::Uuid> FullscreenActionHelper::setCameraIds(
    const QnBusinessRuleViewModel* model,
    const QSet<nx::Uuid>& cameraIds)
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

QSet<nx::Uuid> FullscreenActionHelper::layoutIds(const QnBusinessRuleViewModel* model)
{
    return toIds(getLayoutsInternal(model));
}

QSet<nx::Uuid> FullscreenActionHelper::setLayouts(
    const QnBusinessRuleViewModel* model,
    const QnLayoutResourceList& layouts)
{
    return setLayoutIds(model, toIds(layouts));
}

QSet<nx::Uuid> FullscreenActionHelper::setLayoutIds(
    const QnBusinessRuleViewModel* model,
    const QSet<nx::Uuid>& layoutIds)
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
        [camera](const auto& layout)
        {
            return resourceBelongsToLayout(camera, layout);
        });
}

} // namespace nx::vms::client::desktop
