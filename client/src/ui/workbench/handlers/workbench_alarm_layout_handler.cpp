
#include "workbench_alarm_layout_handler.h"

#include <api/runtime_info_manager.h>
#include <nx/streaming/archive_stream_reader.h>
#include <utils/aspect_ratio.h>

#include <business/actions/abstract_business_action.h>

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

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>

#include <ui/graphics/items/resource/resource_widget.h>

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <nx/streaming/archive_stream_reader.h>

#include <nx/streaming/archive_stream_reader.h>
#include <utils/aspect_ratio.h>
#include <utils/common/delayed.h>

namespace {
    class QnAlarmLayoutResource: public QnLayoutResource {
        Q_DECLARE_TR_FUNCTIONS(QnAlarmLayoutResource)
    public:
        QnAlarmLayoutResource():
            QnLayoutResource()
        {
            NX_ASSERT(qnResPool->getResources<QnAlarmLayoutResource>().isEmpty(), Q_FUNC_INFO, "The Alarm Layout must exist in a single instance");

            setId(QnUuid::createUuid());
            addFlags(Qn::local);
            setName(tr("Alarms"));
            setCellSpacing(0.1, 0.1);
            setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission));
            setUserCanEdit(true);
        }
    };

    typedef QnSharedResourcePointer<QnAlarmLayoutResource> QnAlarmLayoutResourcePtr;
    typedef QnSharedResourcePointerList<QnAlarmLayoutResource> QnAlarmLayoutResourceList;

    /* Processing actions are cleaned by this timeout. */
    const qint64 kProcessingActionTimeoutMs = 5000;
}

QnWorkbenchAlarmLayoutHandler::QnWorkbenchAlarmLayoutHandler(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    connect( action(QnActions::OpenInAlarmLayoutAction), &QAction::triggered, this,   [this] {
        QnActionParameters parameters = menu()->currentParameters(sender());
        openCamerasInAlarmLayout(parameters.resources().filtered<QnVirtualCameraResource>(), true);
    } );

    const auto messageProcessor = QnClientMessageProcessor::instance();

    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this, [this](const QnAbstractBusinessActionPtr &businessAction) {
        if (businessAction->actionType() != QnBusiness::ShowOnAlarmLayoutAction)
            return;

        if (!context()->user())
            return;

        const auto params = businessAction->getParams();

        QnUserResourceList users = qnResPool->getResources<QnUserResource>(params.additionalResources);
        if (!users.isEmpty() && !users.contains(context()->user()))
            return;

        QnVirtualCameraResourceList targetCameras = qnResPool->getResources<QnVirtualCameraResource>(businessAction->getResources());
        if (businessAction->getParams().useSource)
            targetCameras << qnResPool->getResources<QnVirtualCameraResource>(businessAction->getSourceResources());

        if (targetCameras.isEmpty())
            return;

        ActionKey key(businessAction->getBusinessRuleId(), businessAction->getRuntimeParams().eventTimestampUsec);
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

void QnWorkbenchAlarmLayoutHandler::openCamerasInAlarmLayout( const QnVirtualCameraResourceList &cameras, bool switchToLayout ) {
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
        QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
        streamSynchronizer->setState(syncDisabled);
    }
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(syncDisabled));

    // Sort items to guarantee the same item placement for the same set of cameras.
    QnVirtualCameraResourceList sortedCameras = cameras;
    std::sort(sortedCameras.begin(), sortedCameras.end(), [](const QnVirtualCameraResourcePtr &camera1, const QnVirtualCameraResourcePtr &camera2) {
        return camera1->getId() < camera2->getId();
    });

    for (const QnVirtualCameraResourcePtr &camera: sortedCameras)
    {
        auto existingItems = layout->items(camera->getUniqueId());

        /* If the camera is already on layout, just take it to LIVE */
        if (!existingItems.isEmpty())
        {
            for (auto item: existingItems)
                jumpToLive(layout, item);

            continue;
        }

        QnLayoutItemData data;
        data.resource.id = camera->getId();
        data.resource.path = camera->getUniqueId();
        data.uuid = QnUuid::createUuid();
        data.flags = Qn::PendingGeometryAdjustment;

        QString forcedRotation = camera->getProperty(QnMediaResource::rotationKey());
        if (!forcedRotation.isEmpty())
            data.rotation = forcedRotation.toInt();

        if (layout->resource())
            layout->resource()->addItem(data);
    }

    if (switchToLayout)
        workbench()->setCurrentLayout(layout);


    if (!wasEmptyLayout || sortedCameras.empty())
        return;

    for(auto widget: display()->widgets(sortedCameras.first()))
    {
        const auto aspect = widget->visualChannelAspectRatio();
        layout->setCellAspectRatio(QnAspectRatio::closestStandardRatio(aspect).toFloat());
        break;  // Break after first camera aspect set
    }
}

QnWorkbenchLayout* QnWorkbenchAlarmLayoutHandler::findOrCreateAlarmLayout() {
    if (!context()->user())
        return nullptr;

    QnAlarmLayoutResourcePtr alarmLayout;

    QnAlarmLayoutResourceList layouts = qnResPool->getResources<QnAlarmLayoutResource>();
    NX_ASSERT(layouts.size() < 2, Q_FUNC_INFO, "There must be only one alarm layout, if any");
    if (!layouts.empty()) {
        alarmLayout = layouts.first();
    } else {
        alarmLayout = QnAlarmLayoutResourcePtr(new QnAlarmLayoutResource());
        alarmLayout->setParentId(context()->user()->getId());
        qnResPool->addResource(alarmLayout);
    }

    QnWorkbenchLayout* workbenchAlarmLayout = QnWorkbenchLayout::instance(QnLayoutResourcePtr(alarmLayout));
    if (!workbenchAlarmLayout) {
        workbenchAlarmLayout = new QnWorkbenchLayout(alarmLayout, workbench());
        workbenchAlarmLayout->setData(Qt::DecorationRole, qnSkin->icon("titlebar/alert.png"));
        workbench()->addLayout(workbenchAlarmLayout);
    }

    return workbenchAlarmLayout;
}

bool QnWorkbenchAlarmLayoutHandler::alarmLayoutExists() const {
    if (!context()->user())
        return false;

    QnAlarmLayoutResourceList layouts = qnResPool->getResources<QnAlarmLayoutResource>();
    NX_ASSERT(layouts.size() < 2, Q_FUNC_INFO, "There must be only one alarm layout, if any");
    if (layouts.empty())
        return false;

    return QnWorkbenchLayout::instance(QnLayoutResourcePtr(layouts.first())) != nullptr;
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
            //resourceDisplay->archiveReader()->pauseMedia(); //TODO: #GDM make sure this magic is required
            resourceDisplay->archiveReader()->setSpeed(1.0);
            resourceDisplay->archiveReader()->jumpTo(DATETIME_NOW, 0);
            resourceDisplay->archiveReader()->resumeMedia();
        }
        resourceDisplay->start();
    }
}

bool QnWorkbenchAlarmLayoutHandler::currentInstanceIsMain() const
{
    auto clientInstanceManager = qnCommon->instance<QnClientInstanceManager>();
    NX_ASSERT(clientInstanceManager, Q_FUNC_INFO, "Instance Manager must exist here");
    if (!clientInstanceManager)
        return true;

    auto runningInstances = clientInstanceManager->runningInstancesIndices();

    /* Check if we have no access to other instances. */
    if (runningInstances.isEmpty())
        return true;

    QnUuid localUserId = qnRuntimeInfoManager->localInfo().data.userId;

    QSet<QnUuid> connectedInstances;
    for (const QnPeerRuntimeInfo &info: qnRuntimeInfoManager->items()->getItems())
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
