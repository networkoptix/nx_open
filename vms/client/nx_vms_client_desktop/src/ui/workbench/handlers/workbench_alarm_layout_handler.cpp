// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_alarm_layout_handler.h"

#include <QtWidgets/QAction>

#include <api/runtime_info_manager.h>
#include <business/business_resource_validation.h>
#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/qset.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/layouts/layout_factory.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

/* Processing actions are cleaned by this timeout. */
const qint64 kProcessingActionTimeoutMs = 5000;

}

QnWorkbenchAlarmLayoutHandler::QnWorkbenchAlarmLayoutHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(action::OpenInAlarmLayoutAction), &QAction::triggered, this,
        [this]
        {
            const auto parameters = menu()->currentParameters(sender());
            auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
            cameras = accessController()->filtered(cameras, Qn::ViewContentPermission);
            if (!cameras.isEmpty())
                openCamerasInAlarmLayout(cameras, /*switchToLayoutNeeded*/ true);
        });

    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this]()
        {
            if (m_alarmLayoutId.isNull())
                return;

            auto alarmLayout = resourcePool()->getResourceById<QnLayoutResource>(m_alarmLayoutId);
            if (const auto wbLayout = QnWorkbenchLayout::instance(alarmLayout))
            {
                workbench()->removeLayout(wbLayout);
                resourcePool()->removeResource(wbLayout->resource());
                m_alarmLayoutId = {};
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
                targetCameras << resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                    action->getSourceResources(resourcePool()));
            targetCameras = nx::utils::toQSet(
                accessController()->filtered(targetCameras, Qn::ViewContentPermission)).values();

            if (targetCameras.isEmpty())
                return;

            ActionKey key(action->getRuleId(), action->getRuntimeParams().eventTimestampUsec);
            if (m_processingActions.contains(key))
                return; /* See m_processingActions comment. */

            m_processingActions.append(key);
            executeDelayedParented([this, key] {
                m_processingActions.removeOne(key);
            }, kProcessingActionTimeoutMs, this);

            using namespace std::chrono;

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

    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            const bool contains = std::any_of(resources.begin(), resources.end(),
                [this](const QnResourcePtr& resource)
                {
                    return resource->getId() == m_alarmLayoutId;
                });

            if (contains)
                m_alarmLayoutId = {};
        });
}

QnWorkbenchAlarmLayoutHandler::~QnWorkbenchAlarmLayoutHandler()
{}

void QnWorkbenchAlarmLayoutHandler::disableSyncForLayout(QnWorkbenchLayout* layout)
{
    const auto syncDisabled = QnStreamSynchronizationState();
    if (workbench()->currentLayout() == layout)
    {
        auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
        streamSynchronizer->setState(syncDisabled);
    }
    layout->setData(Qn::LayoutSyncStateRole,
        QVariant::fromValue<QnStreamSynchronizationState>(syncDisabled));
}

QnVirtualCameraResourceList QnWorkbenchAlarmLayoutHandler::sortCameraResourceList(
    const QnVirtualCameraResourceList& cameraList)
{
    QnVirtualCameraResourceList sortedCameras = cameraList;
    std::sort(sortedCameras.begin(), sortedCameras.end(),
        [](const QnVirtualCameraResourcePtr& camera1, const QnVirtualCameraResourcePtr& camera2)
        {
            return camera1->getId() < camera2->getId();
        });

    return sortedCameras;
}

void QnWorkbenchAlarmLayoutHandler::stopShowReelIfRunning()
{
    if (action(action::ToggleLayoutTourModeAction)->isChecked())
        menu()->trigger(action::ToggleLayoutTourModeAction);
}

void QnWorkbenchAlarmLayoutHandler::switchToLayout(QnWorkbenchLayout* layout)
{
    stopShowReelIfRunning();

    workbench()->setCurrentLayout(layout);
}

void QnWorkbenchAlarmLayoutHandler::adjustLayoutCellAspectRatio(QnWorkbenchLayout* layout)
{
    for (auto widget: display()->widgets(layout->resource()))
    {
        const auto aspect = widget->visualChannelAspectRatio();
        layout->setCellAspectRatio(QnAspectRatio::closestStandardRatio(aspect).toFloat());
        break; // Break after first camera aspect set.
    }
}

void QnWorkbenchAlarmLayoutHandler::addCameraOnLayout(QnWorkbenchLayout* layout,
    QnVirtualCameraResourcePtr camera)
{
    if (!layout->resource())
        return;

    auto data = layoutItemFromResource(camera);
    data.flags = Qn::PendingGeometryAdjustment;

    layout->resource()->addItem(data);
}

void QnWorkbenchAlarmLayoutHandler::openCamerasInAlarmLayout(
    const QnVirtualCameraResourceList& cameras,
    bool switchToLayoutNeeded,
    qint64 positionUs)
{
    if (cameras.isEmpty())
        return;

    auto alarmLayout = findOrCreateAlarmLayout();
    if (!alarmLayout)
        return;

    const bool wasEmptyLayout = alarmLayout->items().isEmpty();

    disableSyncForLayout(alarmLayout);

    // Sort items before iterate to guarantee the same item placement for the same set of cameras.
    for (const auto& camera: sortCameraResourceList(cameras))
    {
        auto existingItems = alarmLayout->items(camera);

        if (existingItems.isEmpty())
            addCameraOnLayout(alarmLayout, camera);

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
        switchToLayout(alarmLayout);
}

QnWorkbenchLayout* QnWorkbenchAlarmLayoutHandler::findOrCreateAlarmLayout()
{
    // Forbidden when we are logged out
    if (!context()->user())
        return nullptr;

    auto alarmLayout = resourcePool()->getResourceById<QnLayoutResource>(m_alarmLayoutId);
    if (!alarmLayout)
    {
        alarmLayout.reset(new QnLayoutResource());
        alarmLayout->addFlags(Qn::local);
        alarmLayout->setIdUnsafe(QnUuid::createUuid());
        m_alarmLayoutId = alarmLayout->getId();
        resourcePool()->addResource(alarmLayout);
        alarmLayout->setName(tr("Alarms"));
        alarmLayout->setCellSpacing(QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Small));
        alarmLayout->setData(Qn::LayoutIconRole, qnSkin->icon("layouts/alarm.png"));
        alarmLayout->setData(Qn::LayoutPermissionsRole,
            static_cast<int>(Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission));
    }

    auto workbenchAlarmLayout = QnWorkbenchLayout::instance(alarmLayout);
    if (!workbenchAlarmLayout)
    {
        // If user have closed alarm layout, all cameras must be removed.
        alarmLayout->setItems(QnLayoutItemDataMap());
        workbenchAlarmLayout = qnWorkbenchLayoutsFactory->create(alarmLayout, workbench());
        workbench()->addLayout(workbenchAlarmLayout);
    }

    return workbenchAlarmLayout;
}

bool QnWorkbenchAlarmLayoutHandler::alarmLayoutExists() const
{
    if (!context()->user())
        return false;

    if (m_alarmLayoutId.isNull())
        return false;

    auto alarmLayout = resourcePool()->getResourceById<QnLayoutResource>(m_alarmLayoutId);
    if (!alarmLayout)
        return false;

    return QnWorkbenchLayout::instance(alarmLayout);
}

void QnWorkbenchAlarmLayoutHandler::setCameraItemPosition(QnWorkbenchLayout *layout,
    QnWorkbenchItem *item, qint64 positionUs)
{
    NX_ASSERT(layout && item, "Objects must exist here");
    if (!item)
        return;

    using namespace std::chrono;

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

bool QnWorkbenchAlarmLayoutHandler::currentInstanceIsMain() const
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
