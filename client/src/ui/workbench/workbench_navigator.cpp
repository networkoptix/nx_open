#include "workbench_navigator.h"

#include <cassert>

#include <ui/graphics/items/resource_widget.h>

#include "workbench_display.h"

QnWorkbenchNavigator::QnWorkbenchNavigator(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_timeSlider(NULL),
    m_display(display)
{
    assert(display != NULL);
    assert(timeSlider != NULL);


}
    
QnWorkbenchNavigator::~QnWorkbenchNavigator() {
    return;
}

QnTimeSlider *QnWorkbenchNavigator::timeSlider() const {
    return m_timeSlider;
}

void QnWorkbenchNavigator::setTimeSlider(QnTimeSlider *timeSlider) {
    if(m_timeSlider == timeSlider)
        return;

    if(m_timeSlider)
        deinitialize();

    m_timeSlider = timeSlider;

    if(m_timeSlider)
        initialize();
}


void QnWorkbenchNavigator::initialize() {
    assert(m_timeSlider);


} 

void QnWorkbenchNavigator::deinitialize() {
    assert(m_timeSlider);


}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchNavigator::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    /* Update navigation item's target. */
    QnResourceWidget *targetWidget = m_display->widget(QnWorkbench::ZoomedRole);
    if(targetWidget == NULL)
        targetWidget = m_display->widget(QnWorkbench::RaisedRole);
    
//    m_sliderItem->setVideoCamera(targetWidget == NULL ? NULL : targetWidget->display()->camera());
}

void QnWorkbenchNavigator::at_display_widgetAdded(QnResourceWidget *widget) {
    if(QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>()) {
        connect(widget, SIGNAL(motionRegionSelected(QnResourcePtr, QnAbstractArchiveReader *, QList<QRegion>)), this, SLOT(at_widget_motionRegionSelected(QnResourcePtr, QnAbstractArchiveReader*, QList<QRegion>)));
        //connect(m_sliderItem, SIGNAL(clearMotionSelection()), widget, SLOT(clearMotionSelection()));
        //m_sliderItem->addReserveCamera(widget->display()->camera());
    }
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>())
        ;//m_sliderItem->removeReserveCamera(widget->display()->camera());
}