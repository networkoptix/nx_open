#include "workbench_alarm_layout_handler.h"

#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>

#include <ui/workbench/workbench_context.h>


QnWorkbenchAlarmLayoutHandler::QnWorkbenchAlarmLayoutHandler(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    connect( action(Qn::OpenInAlarmLayoutAction), &QAction::triggered, this,   [this] {
        QnActionParameters parameters = menu()->currentParameters(sender());
        openCamerasInAlarmLayout(parameters.resources().filtered<QnVirtualCameraResource>());
    } );

}

QnWorkbenchAlarmLayoutHandler::~QnWorkbenchAlarmLayoutHandler() 
{}

void QnWorkbenchAlarmLayoutHandler::openCamerasInAlarmLayout( const QnVirtualCameraResourceList &cameras ) {

    menu()->trigger(Qn::OpenInNewLayoutAction, cameras);
}
