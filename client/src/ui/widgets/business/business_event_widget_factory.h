#ifndef BUSINESS_EVENT_WIDGET_FACTORY_H
#define BUSINESS_EVENT_WIDGET_FACTORY_H

#include <QtGui/QWidget>

#include <events/abstract_business_event.h>

class QnBusinessEventWidgetFactory {
public:
    static QWidget* createWidget(BusinessEventType::Value eventType, QWidget* parent = 0);
};

#endif // BUSINESS_EVENT_WIDGET_FACTORY_H
