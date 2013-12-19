#include "workbench_fisheye_handler.h"

#include <ui/actions/action_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>


QnWorkbenchFisheyeHandler::QnWorkbenchFisheyeHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::PtzCalibrateFisheyeAction), &QAction::triggered, this, &QnWorkbenchFisheyeHandler::at_ptzCalibrateFisheyeAction_triggered);
}

QnWorkbenchFisheyeHandler::~QnWorkbenchFisheyeHandler() {
    return;
}

void QnWorkbenchFisheyeHandler::at_ptzCalibrateFisheyeAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;

    // TODO: #VASILENKO
}

