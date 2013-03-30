#ifndef BUSINESS_ACTION_WIDGET_FACTORY_H
#define BUSINESS_ACTION_WIDGET_FACTORY_H

#include <QtGui/QWidget>

#include <business/actions/abstract_business_action.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnBusinessActionWidgetFactory {
public:
    static QnAbstractBusinessParamsWidget* createWidget(BusinessActionType::Value actionType, QWidget* parent = 0, QnWorkbenchContext *context = NULL);
};

#endif // BUSINESS_ACTION_WIDGET_FACTORY_H
