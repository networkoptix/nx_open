#include "business_event_rule.h"
#include "recording_business_action.h"

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

    if (bEvent->getToggleState() != ToggleState_NotDefined)
    {
        if (result->getToggleState() != ToggleState_NotDefined){
            ToggleState value = tState != ToggleState_NotDefined ? tState : bEvent->getToggleState();
            result->setToggleState(value);
        }
        else if (bEvent->getToggleState() == ToggleState_Off) {
            return QnAbstractBusinessActionPtr(); // do not generate action (for example: if motion start/stop, send alert on start only)
        }
    }
    else if (result->getToggleState() != ToggleState_NotDefined)
    {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Toggle action MUST used with toggle events only!!!");
    }

    m_actionInProgress = result && result->getToggleState() == ToggleState_On;
    result->setBusinessRuleId(getId());

    return result;
}

QnBusinessParams QnBusinessEventRule::getBusinessParams() const
{
    return m_actionParams;
}

void QnBusinessEventRule::setBusinessParams(const QnBusinessParams &params)
{
    m_actionParams = params;
}

bool QnBusinessEventRule::isActionInProgress() const
{
    return m_actionInProgress;
}

QString QnBusinessEventRule::getUniqueId() const
{
    return QString(QLatin1String("QnBusinessEventRule_")) + getId().toString();
}
