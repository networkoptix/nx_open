#include "workbench_motion_display_watcher.h"
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnWorkbenchMotionDisplayWatcher::QnWorkbenchMotionDisplayWatcher(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display)
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    connect(display,    SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,    SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));

    foreach(QnResourceWidget *widget, display->widgets())
        at_display_widgetAdded(widget);
}

QnWorkbenchMotionDisplayWatcher::~QnWorkbenchMotionDisplayWatcher() {
    if(m_display)
        foreach(QnResourceWidget *widget, m_display.data()->widgets())
            at_display_widgetAboutToBeRemoved(widget);
}


void QnWorkbenchMotionDisplayWatcher::at_display_widgetAdded(QnResourceWidget *widget) {
    connect(widget,     SIGNAL(displayFlagsChanged()),                      this,   SLOT(at_widget_displayFlagsChanged()));

    at_widget_displayFlagsChanged(widget, widget->displayFlags());
}

void QnWorkbenchMotionDisplayWatcher::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    disconnect(widget, NULL, this, NULL);

    at_widget_displayFlagsChanged(widget, 0);
}

void QnWorkbenchMotionDisplayWatcher::at_widget_displayFlagsChanged(QnResourceWidget *widget, int displayFlags) {
    bool oldDisplayed = isMotionGridDisplayed();

    if(displayFlags & QnResourceWidget::DisplayMotion) {
        m_widgets.insert(widget);
    } else {
        m_widgets.remove(widget);
    }

    bool newDisplayed = isMotionGridDisplayed();

    if(oldDisplayed != newDisplayed) {
        if(newDisplayed) {
            emit motionGridShown();
        } else {
            emit motionGridHidden();
        }
    }
}

void QnWorkbenchMotionDisplayWatcher::at_widget_displayFlagsChanged() {
    QnResourceWidget *widget = checked_cast<QnResourceWidget *>(sender());
    at_widget_displayFlagsChanged(widget, widget->displayFlags());
}

