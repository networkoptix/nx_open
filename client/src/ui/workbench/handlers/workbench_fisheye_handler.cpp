#include "workbench_fisheye_handler.h"

#include <ui/actions/action_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include "camera/resource_display.h"
#include "camera/cam_display.h"

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
    connect(&m_calibrator, SIGNAL(finished()), this, SLOT(at_analyseFinished()));
}

QnWorkbenchFisheyeHandler::~QnWorkbenchFisheyeHandler() {
    return;
}

void QnWorkbenchFisheyeHandler::at_ptzCalibrateFisheyeAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;

    QImage screenshot = widget->display()->camDisplay()->getGrayscaleScreenshot(0);
    m_resource = widget->resource();
    m_calibrator.analyseFrameAsync(screenshot);

    // TODO: #VASILENKO

    /*
     
     This is how we install an overlay:

     widget->addOverlayWidget(new FisheyeCalibrationWidget(widget), QnResourceWidget::Visible);

     Don't forget to delete it when done. 

     */
}

void QnWorkbenchFisheyeHandler::at_analyseFinished()
{
    if (!m_resource)
        return;

    QnMediaDewarpingParams params = m_resource->getDewarpingParams();
    params.xCenter = m_calibrator.center().x();
    params.yCenter = m_calibrator.center().y();
    params.radius = m_calibrator.radius();
    m_resource->setDewarpingParams(params);
}
