#ifndef BUSINESS_EVENT_WIDGET_FACTORY_H
#define BUSINESS_EVENT_WIDGET_FACTORY_H

#include <QtGui/QWidget>

#include <ui/widgets/business/abstract_business_event_widget.h>
#include <events/abstract_business_event.h>

class QnBusinessEventWidgetFactory {
public:
    static QnAbstractBusinessEventWidget* createWidget(BusinessEventType::Value eventType, QWidget* parent = 0);
};

#endif // BUSINESS_EVENT_WIDGET_FACTORY_H
