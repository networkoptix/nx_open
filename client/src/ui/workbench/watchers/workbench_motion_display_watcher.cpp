#include "workbench_motion_display_watcher.h"
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnWorkbenchMotionDisplayWatcher::QnWorkbenchMotionDisplayWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(display(),  SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));

    foreach(QnResourceWidget *widget, display()->widgets())
        at_display_widgetAdded(widget);
}

QnWorkbenchMotionDisplayWatcher::~QnWorkbenchMotionDisplayWatcher() {
    foreach(QnResourceWidget *widget, display()->widgets())
        at_display_widgetAboutToBeRemoved(widget);
}


void QnWorkbenchMotionDisplayWatcher::at_display_widgetAdded(QnResourceWidget *widget) {
    connect(widget,     SIGNAL(optionsChanged()),                      this,   SLOT(at_widget_optionsChanged()));

    at_widget_optionsChanged(widget, widget->options());
}

void QnWorkbenchMotionDisplayWatcher::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    disconnect(widget, NULL, this, NULL);

    at_widget_optionsChanged(widget, 0);
}

void QnWorkbenchMotionDisplayWatcher::at_widget_optionsChanged(QnResourceWidget *widget, int options) {
    bool oldDisplayed = isMotionGridDisplayed();

    if(options & QnResourceWidget::DisplayMotion) {
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

void QnWorkbenchMotionDisplayWatcher::at_widget_optionsChanged() {
    QnResourceWidget *widget = checked_cast<QnResourceWidget *>(sender());
    at_widget_optionsChanged(widget, widget->options());
}

