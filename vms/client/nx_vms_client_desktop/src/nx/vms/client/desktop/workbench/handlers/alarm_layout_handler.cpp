// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "alarm_layout_handler.h"

#include <chrono>

#include <QtCore/QList>
#include <QtGui/QAction>

#include <api/runtime_info_manager.h>
#include <business/business_resource_validation.h>
#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client/client_message_processor.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/datetime.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

#include "notification_action_executor.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::rules;
using namespace std::chrono;

namespace {

/* Processing actions are cleaned by this timeout. */
constexpr auto kProcessingActionTimeout = 5s;

QnVirtualCameraResourceList sortedCameras(QnVirtualCameraResourceList cameraList)
{
    std::sort(cameraList.begin(), cameraList.end(),
        [](const QnVirtualCameraResourcePtr& camera1, const QnVirtualCameraResourcePtr& camera2)
        {
            return camera1->getId() < camera2->getId();
        });

    return cameraList;
}

} // namespace

struct ActionKey
{
    QnUuid ruleId;
    qint64 timestampUs = 0;

    bool operator==(const ActionKey& other) const = default;
};

struct AlarmLayoutHandler::Private
{
    /**
     * Actions that are currently handled.
     * Current event rules actions implementation will send us 'Alarm' action once for every camera
     * that is enumerated in action resources. But on the client side we are handling all cameras
     * at once to make sure they will be displayed on layout in the same order. So we are keeping
     * list of all recently handled actions to avoid duplicated method calls.
     *
     * List is cleaned up on timer.
     */
    QList<ActionKey> processingActions;

    LayoutResourcePtr alarmLayout;
};

AlarmLayoutHandler::AlarmLayoutHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private())
{
    connect(action(menu::OpenInAlarmLayoutAction), &QAction::triggered, this,
        [this]
        {
            const auto parameters = menu()->currentParameters(sender());
            const auto cameras = parameters.resources().filtered<QnVirtualCameraResource>(
                [this](const auto& camera)
                {
                    return systemContext()->accessController()->hasPermissions(camera,
                        Qn::ViewContentPermission);
                });
            if (!cameras.isEmpty())
                openCamerasInAlarmLayout(cameras, /*switchToLayoutNeeded*/ true, DATETIME_NOW);
        });

    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this]()
        {
            if (d->alarmLayout)
            {
                workbench()->removeLayout(d->alarmLayout);
                d->alarmLayout.clear();
            }
        });

    const auto messageProcessor = qnClientMessageProcessor;

    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        [this](const vms::event::AbstractActionPtr& action)
        {
            if (action->actionType() != vms::api::ActionType::showOnAlarmLayoutAction)
                return;

            if (!context()->user())
                return;

            const auto params = action->getParams();
            const auto runtimeParams = action->getRuntimeParams();

            /* Skip action if it contains list of users and we are not on the list. */
            if (!QnBusiness::actionAllowedForUser(action, context()->user()))
                return;

            auto targetCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                action->getResources());
            if (action->getParams().useSource)
            {
                targetCameras << resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                    action->getSourceResources(resourcePool()));
            }

            // Remove duplicates.
            std::sort(targetCameras.begin(), targetCameras.end());
            targetCameras.erase(std::unique(targetCameras.begin(), targetCameras.end()),
                targetCameras.end());

            targetCameras = targetCameras.filtered(
                [this](const auto& camera)
                {
                    return systemContext()->accessController()->hasPermissions(camera,
                        Qn::ViewContentPermission);
                });

            if (targetCameras.isEmpty())
                return;

            ActionKey key{action->getRuleId(), action->getRuntimeParams().eventTimestampUsec};
            if (d->processingActions.contains(key))
                return; /* See d->processingActions comment. */

            d->processingActions.append(key);
            executeDelayedParented(
                [this, key] { d->processingActions.removeOne(key); },
                kProcessingActionTimeout,
                this);

            bool switchToLayoutNeeded = params.forced;
            int rewindForMs = params.recordBeforeMs;
            auto camerasPositionUs = (rewindForMs == 0)
                ? DATETIME_NOW
                : duration_cast<microseconds>(
                    microseconds(runtimeParams.eventTimestampUsec) - milliseconds(rewindForMs)
                ).count();

            if ((params.forced && currentInstanceIsMain()) || alarmLayoutExists())
                openCamerasInAlarmLayout(targetCameras, switchToLayoutNeeded, camerasPositionUs);
        });

    connect(
        context()->instance<NotificationActionExecutor>(),
        &NotificationActionExecutor::notificationActionReceived,
        this,
        &AlarmLayoutHandler::onNotificationActionReceived);
}

AlarmLayoutHandler::~AlarmLayoutHandler()
{
}

void AlarmLayoutHandler::onNotificationActionReceived(
    const QSharedPointer<nx::vms::rules::NotificationActionBase>& action)
{
    if (action->type() != rules::utils::type<ShowOnAlarmLayoutAction>())
        return;

    if (!context()->user())
        return;

    auto showOnAlarmLayoutAction = action.dynamicCast<ShowOnAlarmLayoutAction>();

    const auto targetCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        showOnAlarmLayoutAction->deviceIds());

    if (targetCameras.isEmpty())
        return;

    ActionKey key{showOnAlarmLayoutAction->ruleId(), showOnAlarmLayoutAction->timestamp().count()};
    if (d->processingActions.contains(key))
        return; /* See d->processingActions comment. */

    d->processingActions.append(key);
    executeDelayedParented(
        [this, key] { d->processingActions.removeOne(key); },
        kProcessingActionTimeout,
        this);

    const auto rewindFor = showOnAlarmLayoutAction->playbackTime();
    const auto camerasPositionUs = (rewindFor.count() <= 0)
        ? DATETIME_NOW
        : (showOnAlarmLayoutAction->timestamp() - rewindFor).count();

    if ((showOnAlarmLayoutAction->forceOpen() && currentInstanceIsMain()) || alarmLayoutExists())
        openCamerasInAlarmLayout(targetCameras, showOnAlarmLayoutAction->forceOpen(), camerasPositionUs);
}

void AlarmLayoutHandler::disableSyncForLayout(QnWorkbenchLayout* layout)
{
    if (workbench()->currentLayout() == layout)
    {
        auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
        streamSynchronizer->setState(StreamSynchronizationState::disabled());
    }
    layout->setStreamSynchronizationState(StreamSynchronizationState::disabled());
}

void AlarmLayoutHandler::adjustLayoutCellAspectRatio(QnWorkbenchLayout* layout)
{
    for (auto widget: display()->widgets())
    {
        const auto aspect = widget->visualChannelAspectRatio();
        layout->resource()->setCellAspectRatio(
            QnAspectRatio::closestStandardRatio(aspect).toFloat());
        break; // Break after first camera aspect set.
    }
}

void AlarmLayoutHandler::openCamerasInAlarmLayout(
    const QnVirtualCameraResourceList& cameras,
    bool switchToLayoutNeeded,
    qint64 positionUs)
{
    if (cameras.isEmpty())
        return;

    // Stop showreel if it is runing. Should be executed before layout is created because tour
    // stopping clears all existing workbench layouts.
    if (switchToLayoutNeeded)
    {
        if (action(menu::ToggleShowreelModeAction)->isChecked())
            menu()->trigger(menu::ToggleShowreelModeAction);
    }

    auto alarmLayout = findOrCreateAlarmLayout();
    if (!alarmLayout)
        return;

    const bool wasEmptyLayout = alarmLayout->items().isEmpty();

    disableSyncForLayout(alarmLayout);

    // Sort items before iterate to guarantee the same item placement for the same set of cameras.
    for (const auto& camera: sortedCameras(cameras))
    {
        auto existingItems = alarmLayout->items(camera);

        if (existingItems.isEmpty())
        {
            auto data = layoutItemFromResource(camera);
            data.flags = Qn::PendingGeometryAdjustment;
            d->alarmLayout->addItem(data);
        }
        existingItems = alarmLayout->items(camera);

        if (!existingItems.isEmpty())
        {
            for (auto item: existingItems)
                setCameraItemPosition(alarmLayout, item, positionUs);
        }
    }

    if (wasEmptyLayout)
        adjustLayoutCellAspectRatio(alarmLayout);

    if (switchToLayoutNeeded)
        workbench()->setCurrentLayout(alarmLayout);
}

QnWorkbenchLayout* AlarmLayoutHandler::findOrCreateAlarmLayout()
{
    // Forbidden when we are logged out
    if (!context()->user())
        return nullptr;

    if (!d->alarmLayout)
    {
        d->alarmLayout.reset(new LayoutResource());
        d->alarmLayout->addFlags(Qn::local);
        d->alarmLayout->setIdUnsafe(QnUuid::createUuid());
        d->alarmLayout->setName(tr("Alarms"));
        d->alarmLayout->setPredefinedCellSpacing(Qn::CellSpacing::Small);
        d->alarmLayout->setData(Qn::LayoutIconRole, qnSkin->icon("layouts/alarm.png"));
        d->alarmLayout->setData(Qn::LayoutPermissionsRole,
            static_cast<int>(Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission));
    }

    auto workbenchAlarmLayout = workbench()->layout(d->alarmLayout);
    if (!workbenchAlarmLayout)
    {
        // If user have closed alarm layout, all cameras must be removed.
        d->alarmLayout->setItems(common::LayoutItemDataMap());
        workbenchAlarmLayout = workbench()->addLayout(d->alarmLayout);
    }

    return workbenchAlarmLayout;
}

bool AlarmLayoutHandler::alarmLayoutExists() const
{
    if (!context()->user())
        return false;

    if (!d->alarmLayout)
        return false;

    return workbench()->layout(d->alarmLayout) != nullptr;
}

void AlarmLayoutHandler::setCameraItemPosition(
    QnWorkbenchLayout* layout,
    QnWorkbenchItem* item,
    qint64 positionUs)
{
    NX_ASSERT(layout && item, "Objects must exist here");
    if (!item)
        return;

    auto positionMs = (positionUs == DATETIME_NOW)
        ? DATETIME_NOW
        : duration_cast<milliseconds>(microseconds(positionUs)).count();

    /* Set data that will be used on the next layout opening. */
    item->setData(Qn::ItemTimeRole, positionMs);

    /* Update current layout data. */
    if (!layout || workbench()->currentLayout() != layout)
        return;

    /* Navigator will not update values if we are changing current item's state. */
    auto currentWidget = navigator()->currentWidget();
    if (currentWidget && currentWidget->item() == item)
    {
        navigator()->setSpeed(1.0);
        navigator()->setPosition(positionUs);
    }

    if (auto resourceDisplay = display()->display(item))
    {
        if (resourceDisplay->archiveReader())
        {
            // TODO: #sivanov make sure this magic is required:
            //resourceDisplay->archiveReader()->pauseMedia();
            resourceDisplay->archiveReader()->setSpeed(1.0);
            resourceDisplay->archiveReader()->jumpTo(positionUs, 0);
            resourceDisplay->archiveReader()->resumeMedia();
        }
        resourceDisplay->start();
    }
}

bool AlarmLayoutHandler::currentInstanceIsMain() const
{
    auto runningInstancesManager = appContext()->runningInstancesManager();
    if (!NX_ASSERT(runningInstancesManager, "Instance Manager must exist here"))
        return true;

    auto runningInstances = nx::utils::toQSet(runningInstancesManager->runningInstancesGuids());

    // Check if we have no access to other instances.
    if (runningInstances.isEmpty())
        return true;

    QnUuid localUserId = runtimeInfoManager()->localInfo().data.userId;

    QSet<QnUuid> connectedInstances;
    for (const QnPeerRuntimeInfo &info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.userId == localUserId)
            connectedInstances.insert(info.uuid);
    }

    const auto currentInstanceGuid = runningInstancesManager->currentInstanceGuid();
    NX_ASSERT(connectedInstances.contains(currentInstanceGuid),
        "Current instance must be present in the list of connected");

    // Main instance is the one that has the smallest index between all instances connected to the
    // current server with the same credentials.
    runningInstances.intersect(connectedInstances);
    if (!NX_ASSERT(runningInstances.contains(currentInstanceGuid)))
        return true;

    auto connectedFromThisPc = runningInstances.values();
    std::sort(connectedFromThisPc.begin(), connectedFromThisPc.end());
    return connectedFromThisPc.first() == currentInstanceGuid;
}

} // namespace nx::vms::client::desktop
