#include "business_event_rule.h"
#include "recording_business_action.h"
#include "toggle_business_event.h"

QnBusinessEventRule::QnBusinessEventRule()
{
    m_actionInProgress = false;
}

QnAbstractBusinessActionPtr QnBusinessEventRule::getAction(QnAbstractBusinessEventPtr bEvent, ToggleState tState)
{
    QnAbstractBusinessActionPtr result;
    switch (m_actionType)
    {
        case BA_CameraRecording:
            result = QnAbstractBusinessActionPtr(new QnRecordingBusinessAction);
            break;
        case BA_CameraOutput:
        case BA_Bookmark:
        case BA_PanicRecording:
        case BA_SendMail:
        case BA_Alert:
        case BA_ShowPopup:
        default:
            return QnAbstractBusinessActionPtr(); // not implemented
    }

    if (!m_actionParams.isEmpty())
        result->setParams(m_actionParams);
    result->setResource(m_destination);

    QnToggleBusinessEventPtr toggleEvent = bEvent.dynamicCast<QnToggleBusinessEvent>();
    QnToggleBusinessActionPtr toggleAction = result.dynamicCast<QnToggleBusinessAction>();
    if (toggleEvent)
    {
        if (toggleAction){
            ToggleState value = tState != ToggleState_NotDefined ? tState : toggleEvent->getToggleState();
            toggleAction->setToggleState(value);
        }
        else if (toggleEvent->getToggleState() == ToggleState_Off) {
            return QnAbstractBusinessActionPtr(); // do not generate action (for example: if motion start/stop, send alert on start only)
        }
    }
    else if (toggleAction)
    {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Toggle action MUST used with toggle events only!!!");
    }

    m_actionInProgress = toggleAction && toggleAction->getToggleState() == ToggleState_On;

    return result;
}

bool QnBusinessEventRule::isActionInProgress() const
{
    return m_actionInProgress;
}
