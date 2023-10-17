// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_actions_factory.h"

#include <QtGui/QAction>
#include <QtGui/QActionGroup>

#include <camera/camera_data_manager.h>
#include <nx/vms/api/types/storage_location.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/camera/storage_location_camera_controller.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

using StorageLocation = nx::vms::api::StorageLocation;

namespace nx::vms::client::desktop {
namespace menu {

ChunksFilterActionFactory::ChunksFilterActionFactory(Manager* parent):
    base_type(parent)
{
}

QList<QAction*> ChunksFilterActionFactory::newActions(
    const Parameters& parameters,
    QObject* parent)
{
    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);

    auto cameraDataManager = appContext()->currentSystemContext()->cameraDataManager();

    const auto currentMode = cameraDataManager->storageLocation();

    auto addAction =
        [this, actionGroup, parent, currentMode, cameraDataManager]
        (StorageLocation mode, const QString& text)
        {
            auto action = new QAction(parent);
            action->setText(text);
            action->setCheckable(true);
            action->setChecked(currentMode == mode);
            connect(action, &QAction::triggered, this,
                [this, mode, cameraDataManager]
                {
                    cameraDataManager->setStorageLocation(mode);
                    workbenchContext()->instance<StorageLocationCameraController>()
                        ->setStorageLocation(mode);
                });
            actionGroup->addAction(action);
        };

    addAction(StorageLocation::both, tr("No filter"));
    addAction(StorageLocation::main, tr("Main storage only"));
    addAction(StorageLocation::backup, tr("Backup storage only"));

    return actionGroup->actions();
}

} // namespace menu {
} // namespace nx::vms::client::desktop
