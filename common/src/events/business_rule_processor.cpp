#include <QList>
#include "business_rule_processor.h"

void QnBusinessRuleProcessor::processBusinessEvent(QnAbstractBusinessEventPtr bEvent)
{
    QList<QnAbstractBusinessActionPtr> actions = matchActions();
    foreach(QnAbstractBusinessActionPtr action, actions)
        executeAction(action);
}

QList<QnAbstractBusinessActionPtr> QnBusinessRuleProcessor::matchActions()
{
    return QList<QnAbstractBusinessActionPtr>();
}

void QnBusinessRuleProcessor::executeAction(QnAbstractBusinessActionPtr action)
{
    switch(action->actionType())
    {
        case BA_CameraOutput:
            break;
        case BA_SendMail:
            break;
        case BA_Bookmark:
            break;
        case BA_Alert:
            break;
        case BA_CameraRecording:
            break;
        case BA_PanicRecording:
            break;
        default:
            break;
    }
}
