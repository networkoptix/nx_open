#include "workbench_alarm_layout_handler.h"

#include <business/actions/abstract_business_action.h>

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
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>

class QnAlarmLayoutResource: public QnLayoutResource {
public:
    QnAlarmLayoutResource():
        QnLayoutResource(qnResTypePool)
    {
        Q_ASSERT_X(qnResPool->getResources<QnAlarmLayoutResource>().isEmpty(), Q_FUNC_INFO, "The Alarm Layout must exist in a single instance");

        setId(QnUuid::createUuid());
        addFlags(Qn::local);
        setName(tr("Alarms"));
        setCellSpacing(0.1, 0.1);
        setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        setUserCanEdit(true);
    }
};

typedef QnSharedResourcePointer<QnAlarmLayoutResource> QnAlarmLayoutResourcePtr;
typedef QnSharedResourcePointerList<QnAlarmLayoutResource> QnAlarmLayoutResourceList;

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
        QnBusiness::EventType eventType = params.eventType;

        QnVirtualCameraResourceList targetCameras = qnResPool->getResources<QnVirtualCameraResource>(businessAction->getResources());
        if (businessAction->getParams().useSource) {
            if (QnVirtualCameraResourcePtr sourceCamera = qnResPool->getResourceById<QnVirtualCameraResource>(params.eventResourceId))
                targetCameras << sourceCamera;
            targetCameras << qnResPool->getResources<QnVirtualCameraResource>(params.metadata.cameraRefs);
        }

        if (!targetCameras.isEmpty())
            openCamerasInAlarmLayout(targetCameras, businessAction->getParams().forced);
    });

}

QnWorkbenchAlarmLayoutHandler::~QnWorkbenchAlarmLayoutHandler()
{}

void QnWorkbenchAlarmLayoutHandler::openCamerasInAlarmLayout( const QnVirtualCameraResourceList &cameras, bool switchToLayout ) {
    auto layout = findOrCreateAlarmLayout();
    if (!layout)
        return;

    for (const QnVirtualCameraResourcePtr &camera: cameras) {
        if (!layout->items(camera->getUniqueId()).isEmpty())
            continue;

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
