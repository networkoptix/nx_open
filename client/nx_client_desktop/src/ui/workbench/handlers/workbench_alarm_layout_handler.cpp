#include "workbench_alarm_layout_handler.h"

#include <QtWidgets/QAction>

#include <api/runtime_info_manager.h>
#include <nx/streaming/archive_stream_reader.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <business/business_resource_validation.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <common/common_module.h>

#include <client/client_message_processor.h>
#include <client/client_instance_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

#include <ui/graphics/items/resource/resource_widget.h>

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <nx/streaming/archive_stream_reader.h>

#include <nx/streaming/archive_stream_reader.h>
#include <utils/common/delayed.h>
#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>

using namespace nx;
using namespace nx::client::desktop::ui;

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
            openCamerasInAlarmLayout(cameras, true);
        });

    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this]()
        {
            if (!m_alarmLayout)
                return;

            if (const auto wbLayout = QnWorkbenchLayout::instance(m_alarmLayout))
                workbench()->removeLayout(wbLayout);
        });

    const auto messageProcessor = qnClientMessageProcessor;

    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        [this](const vms::event::AbstractActionPtr& action)
        {
            if (action->actionType() != vms::event::showOnAlarmLayoutAction)
                return;

            if (!context()->user())
                return;

            const auto params = action->getParams();

            /* Skip action if it contains list of users and we are not on the list. */
            if (!QnBusiness::actionAllowedForUser(action->getParams(), context()->user()))
                return;

            auto targetCameras = resourcePool()->getResources<QnVirtualCameraResource>(action->getResources());
            if (action->getParams().useSource)
                targetCameras << resourcePool()->getResources<QnVirtualCameraResource>(action->getSourceResources());
            targetCameras = accessController()->filtered(targetCameras, Qn::ViewContentPermission);
            targetCameras = targetCameras.toSet().toList();

            if (targetCameras.isEmpty())
                return;

            ActionKey key(action->getRuleId(), action->getRuntimeParams().eventTimestampUsec);
            if (m_processingActions.contains(key))
                return; /* See m_processingActions comment. */

            m_processingActions.append(key);
            executeDelayedParented([this, key] {
                m_processingActions.removeOne(key);
            }, kProcessingActionTimeoutMs, this);

            /* If forced, open layout instantly */
            if (params.forced)
            {
                if (currentInstanceIsMain())
                    openCamerasInAlarmLayout(targetCameras, true);
            }
            else if (alarmLayoutExists())
                openCamerasInAlarmLayout(targetCameras, false);
        });

}

QnWorkbenchAlarmLayoutHandler::~QnWorkbenchAlarmLayoutHandler()
{}

void QnWorkbenchAlarmLayoutHandler::openCamerasInAlarmLayout(
    const QnVirtualCameraResourceList& cameras,
    bool switchToLayout)
{
    if (cameras.isEmpty())
        return;

    auto layout = findOrCreateAlarmLayout();
    if (!layout)
        return;

    const bool wasEmptyLayout = layout->items().isEmpty();

    /* Disable Sync on layout before adding cameras */
    const auto syncDisabled = QnStreamSynchronizationState();
    if (workbench()->currentLayout() == layout)
    {
        auto streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
        streamSynchronizer->setState(syncDisabled);
    }
    layout->setData(Qn::LayoutSyncStateRole,
        QVariant::fromValue<QnStreamSynchronizationState>(syncDisabled));

    // Sort items to guarantee the same item placement for the same set of cameras.
    QnVirtualCameraResourceList sortedCameras = cameras;
    std::sort(sortedCameras.begin(), sortedCameras.end(),
        [](const QnVirtualCameraResourcePtr& camera1, const QnVirtualCameraResourcePtr& camera2)
        {
            return camera1->getId() < camera2->getId();
        });

    for (const auto& camera: sortedCameras)
    {
        auto existingItems = layout->items(camera);

        /* If the camera is already on layout, just take it to LIVE */
        if (!existingItems.isEmpty())
        {
            for (auto item: existingItems)
                jumpToLive(layout, item);

            continue;
        }

        QnLayoutItemData data;
        data.resource.id = camera->getId();
        data.resource.uniqueId = camera->getUniqueId();
        data.uuid = QnUuid::createUuid();
        data.flags = Qn::PendingGeometryAdjustment;

        QString forcedRotation = camera->getProperty(QnMediaResource::rotationKey());
        if (!forcedRotation.isEmpty())
            data.rotation = forcedRotation.toInt();

        if (layout->resource())
            layout->resource()->addItem(data);
    }

    if (switchToLayout)
    {
        // Stop layout tour if it is running
        if (action(action::ToggleLayoutTourModeAction)->isChecked())
            menu()->trigger(action::ToggleLayoutTourModeAction);

        workbench()->setCurrentLayout(layout);
    }


    if (!wasEmptyLayout || sortedCameras.empty())
        return;

    for (auto widget: display()->widgets(sortedCameras.first()))
    {
        const auto aspect = widget->visualChannelAspectRatio();
        layout->setCellAspectRatio(QnAspectRatio::closestStandardRatio(aspect).toFloat());
        break;  // Break after first camera aspect set
    }
}

QnWorkbenchLayout* QnWorkbenchAlarmLayoutHandler::findOrCreateAlarmLayout()
{
    // Forbidden when we are logged out
    if (!context()->user())
        return nullptr;

    if (!m_alarmLayout)
    {
        m_alarmLayout.reset(new QnLayoutResource(commonModule()));
        m_alarmLayout->setId(QnUuid::createUuid());
        m_alarmLayout->setName(tr("Alarms"));
        m_alarmLayout->setCellSpacing(QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Small));
        m_alarmLayout->setData(Qn::LayoutIconRole, qnSkin->icon("layouts/alarm.png"));
        m_alarmLayout->setData(Qn::LayoutPermissionsRole,
            static_cast<int>(Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission));
    }

    auto workbenchAlarmLayout = QnWorkbenchLayout::instance(m_alarmLayout);
    if (!workbenchAlarmLayout)
    {
        // If user have closed alarm layout, all cameras must be removed.
        m_alarmLayout->setItems(QnLayoutItemDataMap());
        workbenchAlarmLayout = qnWorkbenchLayoutsFactory->create(m_alarmLayout, workbench());
        workbench()->addLayout(workbenchAlarmLayout);
    }

    return workbenchAlarmLayout;
}

bool QnWorkbenchAlarmLayoutHandler::alarmLayoutExists() const
{
    return context()->user() && m_alarmLayout && QnWorkbenchLayout::instance(m_alarmLayout);
}

void QnWorkbenchAlarmLayoutHandler::jumpToLive(QnWorkbenchLayout *layout, QnWorkbenchItem *item ) {
    NX_ASSERT(layout && item, Q_FUNC_INFO, "Objects must exist here");
    if (!item)
        return;

    /* Set data that will be used on the next layout opening. */
    item->setData(Qn::ItemTimeRole, DATETIME_NOW);

    /* Update current layout data. */
    if (!layout || workbench()->currentLayout() != layout)
        return;

    /* Navigator will not update values if we are changing current item's state. */
    auto currentWidget = navigator()->currentWidget();
    if (currentWidget && currentWidget->item() == item)
    {
        navigator()->setSpeed(1.0);
        navigator()->setPosition(DATETIME_NOW);
    }

    if (auto resourceDisplay = display()->display(item))
    {
        if (resourceDisplay->archiveReader())
        {
            //resourceDisplay->archiveReader()->pauseMedia(); // TODO: #GDM make sure this magic is required
            resourceDisplay->archiveReader()->setSpeed(1.0);
            resourceDisplay->archiveReader()->jumpTo(DATETIME_NOW, 0);
            resourceDisplay->archiveReader()->resumeMedia();
        }
        resourceDisplay->start();
    }
}

bool QnWorkbenchAlarmLayoutHandler::currentInstanceIsMain() const
{
    auto clientInstanceManager = qnClientInstanceManager;
    NX_ASSERT(clientInstanceManager, Q_FUNC_INFO, "Instance Manager must exist here");
    if (!clientInstanceManager)
        return true;

    auto runningInstances = clientInstanceManager->runningInstancesIndices();

    /* Check if we have no access to other instances. */
    if (runningInstances.isEmpty())
        return true;

    QnUuid localUserId = runtimeInfoManager()->localInfo().data.userId;

    QSet<QnUuid> connectedInstances;
    for (const QnPeerRuntimeInfo &info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.userId != localUserId)
            continue;
        connectedInstances.insert(info.uuid);
    }

    /* Main instance is the one that has the smallest index between all instances
    connected to current server with the same credentials. */
    for (int index: runningInstances)
    {
        if (index == clientInstanceManager->instanceIndex())
            return true;

        /* Check if other instance connected to the same server. */
        QnUuid otherInstanceUuid = clientInstanceManager->instanceGuidForIndex(index);
        if (connectedInstances.contains(otherInstanceUuid))
            return false;
    }

    return true;
}
