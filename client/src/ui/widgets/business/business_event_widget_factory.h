#ifndef BUSINESS_EVENT_WIDGET_FACTORY_H
#define BUSINESS_EVENT_WIDGET_FACTORY_H

#include <QtGui/QWidget>

#include <events/abstract_business_event.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnBusinessEventWidgetFactory {
public:
    static QnAbstractBusinessParamsWidget* createWidget(BusinessEventType::Value eventType,
                                                        QWidget* parent = 0,
                                                        QnWorkbenchContext *context = NULL);
};

#endif // BUSINESS_EVENT_WIDGET_FACTORY_H
