#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(BusinessActionType::Value actionType, QWidget *parent) {
    switch (actionType) {
    case BusinessActionType::BA_CameraRecording:
        return new QnRecordingBusinessActionWidget(parent);
    case BusinessActionType::BA_SendMail:
        return new QnSendmailBusinessActionWidget(parent);
    default:
        return new QnEmptyBusinessActionWidget(parent);
    }
}
