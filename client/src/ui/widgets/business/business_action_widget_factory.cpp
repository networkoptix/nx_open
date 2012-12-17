#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(BusinessActionType::Value actionType, QWidget *parent) {
    switch (actionType) {
    default:
        return new QnEmptyBusinessActionWidget(parent);
    }
}
