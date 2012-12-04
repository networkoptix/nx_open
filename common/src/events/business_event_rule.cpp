
#include "business_event_rule.h"
#include "recording_business_action.h"
#include "sendmail_business_action.h"


QnBusinessEventRule::QnBusinessEventRule()
:
    m_eventType( BusinessEventType::BE_NotDefined ),
    m_actionType( BusinessActionType::BA_NotDefined ),
    m_actionInProgress( false )
{
    //by default, rule triggers on toggle event start. for example: if motion start/stop, send alert on start only
    m_eventCondition[BusinessEventParameters::toggleState] = QLatin1String(ToggleState::toString(ToggleState::On));
}

QnAbstractBusinessActionPtr QnBusinessEventRule::getAction(QnAbstractBusinessEventPtr bEvent, ToggleState::Value tState)
{
    QnAbstractBusinessActionPtr result;
    switch (m_actionType)
    {
        case BusinessActionType::BA_CameraRecording:
            result = QnAbstractBusinessActionPtr(new QnRecordingBusinessAction);
            break;
        case BusinessActionType::BA_CameraOutput:
        case BusinessActionType::BA_Bookmark:
        case BusinessActionType::BA_PanicRecording:
        case BusinessActionType::BA_SendMail:
            result = QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(bEvent));
            break;
        case BusinessActionType::BA_Alert:
        case BusinessActionType::BA_ShowPopup:
        default:
            return QnAbstractBusinessActionPtr(); // not implemented
    }

    if (!m_actionParams.isEmpty())
        result->setParams(m_actionParams);
    result->setResource(m_destination);

    if (bEvent->getToggleState() != ToggleState::NotDefined)
    {
        if (result->isToggledAction()) {
            ToggleState::Value value = tState != ToggleState::NotDefined ? tState : bEvent->getToggleState();
            result->setToggleState(value);
        }
    }
    else if (result->getToggleState() != ToggleState::NotDefined)
    {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Toggle action MUST used with toggle events only!!!");
    }

    m_actionInProgress = result && result->getToggleState() == ToggleState::On;
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

//ToggleState::Value QnBusinessEventRule::getToggleStateFilter() const
//{
//    return m_toggleStateFilter;
//}
//
//void QnBusinessEventRule::setToggleStateFilter( ToggleState::Value _filter )
//{
//    m_toggleStateFilter = _filter;
//}

bool QnBusinessEventRule::isActionInProgress() const
{
    return m_actionInProgress;
}

QString QnBusinessEventRule::getUniqueId() const
{
    return QString(QLatin1String("QnBusinessEventRule_")) + getId().toString();
}
