#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>
#include <ui/widgets/business/popup_business_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(BusinessActionType::Value actionType, QWidget *parent,
                                                                            QnWorkbenchContext *context) {
    switch (actionType) {
        case BusinessActionType::BA_CameraRecording:
            return new QnRecordingBusinessActionWidget(parent);
        case BusinessActionType::BA_SendMail:
            return new QnSendmailBusinessActionWidget(parent, context);
        case BusinessActionType::BA_ShowPopup:
            return new QnPopupBusinessActionWidget(parent, context);
        default:
            return new QnEmptyBusinessActionWidget(parent);
    }
}
