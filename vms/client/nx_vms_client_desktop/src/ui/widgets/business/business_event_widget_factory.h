// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef BUSINESS_EVENT_WIDGET_FACTORY_H
#define BUSINESS_EVENT_WIDGET_FACTORY_H

#include <QtWidgets/QWidget>

#include <nx/vms/event/events/abstract_event.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnBusinessEventWidgetFactory {
public:
    static QnAbstractBusinessParamsWidget* createWidget(nx::vms::api::EventType eventType,
                                                        QWidget* parent = 0,
                                                        QnWorkbenchContext *context = nullptr);
};

#endif // BUSINESS_EVENT_WIDGET_FACTORY_H
