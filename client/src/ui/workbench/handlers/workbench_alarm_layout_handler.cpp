#include "workbench_alarm_layout_handler.h"

#include <business/actions/abstract_business_action.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

namespace {
    class QnAlarmLayoutResource: public QnLayoutResource {
        Q_DECLARE_TR_FUNCTIONS(QnAlarmLayoutResource)
    public:
        QnAlarmLayoutResource():
            QnLayoutResource(qnResTypePool)
        {
            Q_ASSERT_X(qnResPool->getResources<QnAlarmLayoutResource>().isEmpty(), Q_FUNC_INFO, "The Alarm Layout must exist in a single instance");

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
}

QnWorkbenchAlarmLayoutHandler::QnWorkbenchAlarmLayoutHandler(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    connect( action(Qn::OpenInAlarmLayoutAction), &QAction::triggered, this,   [this] {
        QnActionParameters parameters = menu()->currentParameters(sender());
        openCamerasInAlarmLayout(parameters.resources().filtered<QnVirtualCameraResource>(), true);
    } );

    const auto messageProcessor = QnClientMessageProcessor::instance();

    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this, [this](const QnAbstractBusinessActionPtr &businessAction) {
        if (businessAction->actionType() != QnBusiness::ShowOnAlarmLayoutAction)
            return;

        if (!context()->user())
            return;

        QnUserResourceList users = qnResPool->getResources<QnUserResource>(businessAction->getParams().additionalResources);
        if (!users.isEmpty() && !users.contains(context()->user()))
            return;

        //TODO: #GDM code duplication
        QnBusinessEventParameters params = businessAction->getRuntimeParams();

        QnVirtualCameraResourceList targetCameras = qnResPool->getResources<QnVirtualCameraResource>(businessAction->getResources());
        if (businessAction->getParams().useSource) {
            if (QnVirtualCameraResourcePtr sourceCamera = qnResPool->getResourceById<QnVirtualCameraResource>(params.eventResourceId))
                targetCameras << sourceCamera;
            targetCameras << qnResPool->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
        }

        /* If forced, open layout instantly */
        if (businessAction->getParams().forced)
            openCamerasInAlarmLayout(targetCameras, true);
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

    /* Disable Sync on layout before adding cameras */
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState()));

    // Sort items to guarantee the same item placement for the same set of cameras.
    QnVirtualCameraResourceList sortedCameras = cameras;
    std::sort(sortedCameras.begin(), sortedCameras.end(), [](const QnVirtualCameraResourcePtr &camera1, const QnVirtualCameraResourcePtr &camera2) {
        return camera1->getId() < camera2->getId();
    });

    for (const QnVirtualCameraResourcePtr &camera: sortedCameras) {
        auto existingItems = layout->items(camera->getUniqueId());

        /* If the camera is already on layout, just take it to LIVE */
        if (!existingItems.isEmpty()) {
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
}

QnWorkbenchLayout* QnWorkbenchAlarmLayoutHandler::findOrCreateAlarmLayout() {
    if (!context()->user())
        return nullptr;

    QnAlarmLayoutResourcePtr alarmLayout;

    QnAlarmLayoutResourceList layouts = qnResPool->getResources<QnAlarmLayoutResource>();
    Q_ASSERT_X(layouts.size() < 2, Q_FUNC_INFO, "There must be only one alarm layout, if any");
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
    Q_ASSERT_X(layouts.size() < 2, Q_FUNC_INFO, "There must be only one alarm layout, if any");
    if (layouts.empty())
        return false;

    return QnWorkbenchLayout::instance(QnLayoutResourcePtr(layouts.first())) != nullptr;
}

void QnWorkbenchAlarmLayoutHandler::jumpToLive(QnWorkbenchLayout *layout, QnWorkbenchItem *item ) {
    Q_ASSERT_X(layout && item, Q_FUNC_INFO, "Objects must exist here");
    if (!item)
        return;

    /* Set data that will be used on the next layout opening. */
    item->setData(Qn::ItemTimeRole, DATETIME_NOW);

    /* Update current layout data. */
    if (!layout || workbench()->currentLayout() != layout)
        return;

    if (auto resourceDisplay = display()->display(item)) {
        if (resourceDisplay->camDisplay())
            resourceDisplay->camDisplay()->setSpeed(1.0);
        resourceDisplay->setCurrentTimeUSec(DATETIME_NOW);
        resourceDisplay->start();
    }

}
