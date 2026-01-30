// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type_selection_dialog.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

namespace {

void addDerivedTypes(
    QStringList& types,
    const nx::vms::client::core::analytics::taxonomy::ObjectType* type)
{
    for (const auto& derivedObjectType: type->derivedObjectTypes())
    {
        if (types.contains(derivedObjectType->mainTypeId()))
            continue;

        types.append(derivedObjectType->mainTypeId());
        addDerivedTypes(types, derivedObjectType);
    }
}

QStringList getObjectTypes(
    const nx::vms::client::core::analytics::taxonomy::AnalyticsFilterModel* filterModel)
{
    QStringList typeIds;
    for (const auto& type: filterModel->objectTypes())
    {
        typeIds.append(type->mainTypeId());
        addDerivedTypes(typeIds, type);
    }
    return typeIds;
}

} // namespace

namespace nx::vms::client::desktop {

ObjectTypeSelectionDialog::ObjectTypeSelectionDialog(
    SystemContext* systemContext,
    bool allObjects,
    const QStringList& objectTypeIds,
    const QStringList& availableTypeIds,
    QWidget* parent)
    :
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/Analytics/ObjectTypeSelectionDialog.qml"),
        QVariantMap{
            {"allObjects", allObjects},
            {"objectTypeIds", objectTypeIds},
            {"availableTypeIds", availableTypeIds}},
        parent),
    SystemContextAware(systemContext)
{
}

bool ObjectTypeSelectionDialog::allObjects() const
{
    return rootObjectHolder()->object()->property("allObjects").toBool();
}

QStringList ObjectTypeSelectionDialog::objectTypeIds() const
{
    return rootObjectHolder()->object()->property("objectTypeIds").toStringList();
}

void ObjectTypeSelectionDialog::openSelectionDialog(
    api::ObjectTypeSettings& settings,
    QWidget* parent,
    const std::function<void()>& callback,
    const std::optional<nx::Uuid>& cameraResourceId)
{
    const auto& systemContext = appContext()->currentSystemContext();
    const auto filterModel = std::make_unique<core::analytics::taxonomy::AnalyticsFilterModel>(
        systemContext->taxonomyManager(), parent);

    if (cameraResourceId)
    {
        filterModel->setSelectedDevices({cameraResourceId.value()});
    }
    else
    {
        const auto& list = systemContext->resourcePool()->getAllCameras();
        const QnVirtualCameraResourceSet cameraSet(list.begin(), list.end());
        filterModel->setSelectedDevices(cameraSet);
    }

    auto dialog = new ObjectTypeSelectionDialog(
        systemContext,
        settings.isAllObjectTypes,
        settings.objectTypeIds,
        getObjectTypes(filterModel.get()),
        parent);

    connect(
        dialog,
        &QmlDialogWrapper::done,
        dialog,
        [dialog, &settings, callback](bool accepted)
        {
            if (accepted)
            {
                settings.isAllObjectTypes = dialog->allObjects();
                settings.objectTypeIds = dialog->objectTypeIds();

                if (callback)
                    callback();
            }

            dialog->deleteLater();
        });

    dialog->open();
}

} // namespace nx::vms::client::desktop
