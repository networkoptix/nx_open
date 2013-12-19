#include "workbench_fisheye_handler.h"

#include <ui/actions/action_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

// -------------------------------------------------------------------------- //
// FisheyeCalibrationWidget
// -------------------------------------------------------------------------- //
class FisheyeCalibrationWidget: public GraphicsWidget {
    typedef GraphicsWidget base_type;

public:
    FisheyeCalibrationWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags)
    {
        setAcceptedMouseButtons(0);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        // TODO: #VASILENKO
    }
};


// -------------------------------------------------------------------------- //
// QnWorkbenchFisheyeHandler
// -------------------------------------------------------------------------- //
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

    /*
     
     This is how we install an overlay:

     widget->addOverlayWidget(new FisheyeCalibrationWidget(widget), QnResourceWidget::Visible);

     Don't forget to delete it when done. 

     */
}

