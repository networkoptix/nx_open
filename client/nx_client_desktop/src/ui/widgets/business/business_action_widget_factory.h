#ifndef BUSINESS_ACTION_WIDGET_FACTORY_H
#define BUSINESS_ACTION_WIDGET_FACTORY_H

#include <QtWidgets/QWidget>

#include <nx/vms/event/actions/abstract_action.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnBusinessActionWidgetFactory {
public:
    static QnAbstractBusinessParamsWidget* createWidget(nx::vms::event::ActionType actionType, QWidget* parent = 0);
};

#endif // BUSINESS_ACTION_WIDGET_FACTORY_H
