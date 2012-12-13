#include "business_event_rule.h"

#include "recording_business_action.h"
#include "sendmail_business_action.h"
#include "common_business_action.h"

#include <core/resource/resource.h>


QnBusinessEventRule::QnBusinessEventRule()
:
    m_eventType( BusinessEventType::BE_NotDefined ),
    m_actionType( BusinessActionType::BA_NotDefined )
{
    //by default, rule triggers on toggle event start. for example: if motion start/stop, send alert on start only
    m_eventCondition[BusinessEventParameters::toggleState] = (int)ToggleState::On;
}

QnAbstractBusinessActionPtr QnBusinessEventRule::instantiateAction(QnAbstractBusinessEventPtr bEvent, ToggleState::Value tState) const {
    QnAbstractBusinessActionPtr result;
    switch (m_actionType)
    {
        case BusinessActionType::BA_CameraOutput:
        case BusinessActionType::BA_Bookmark:
        case BusinessActionType::BA_PanicRecording:
            result = QnAbstractBusinessActionPtr(new CommonBusinessAction(m_actionType));
            break;
        case BusinessActionType::BA_CameraRecording:
            result = QnAbstractBusinessActionPtr(new QnRecordingBusinessAction);
            break;
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

    if (BusinessEventType::hasToggleState(bEvent->getEventType()) && BusinessActionType::hasToggleState(m_actionType)) {
        ToggleState::Value value = tState != ToggleState::NotDefined ? tState : bEvent->getToggleState();
        result->setToggleState(value);
    }
    result->setBusinessRuleId(getId());

    return result;
}

QnResourcePtr QnBusinessEventRule::getSrcResource() const {
    return m_source;
}

void QnBusinessEventRule::setSrcResource(QnResourcePtr value) {
    m_source = value;
}

BusinessEventType::Value QnBusinessEventRule::getEventType() const {
    return m_eventType;
}

void QnBusinessEventRule::setEventType(BusinessEventType::Value value) {
    m_eventType = value;
}

BusinessActionType::Value QnBusinessEventRule::getActionType() const {
    return m_actionType;
}

void QnBusinessEventRule::setActionType(BusinessActionType::Value value) {
    m_actionType = value;
}

QnResourcePtr QnBusinessEventRule::getDstResource() const {
    return m_destination;
}

void QnBusinessEventRule::setDstResource(QnResourcePtr value) {
    m_destination = value;
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

QString QnBusinessEventRule::getUniqueId() const
{
    return QString(QLatin1String("QnBusinessEventRule_")) + getId().toString();
}

ToggleState::Value QnBusinessEventRule::getEventToggleState() const {
    QnBusinessParams::const_iterator paramIter = m_eventCondition.find( BusinessEventParameters::toggleState);
    if( paramIter == m_eventCondition.end() )
        return ToggleState::NotDefined;
    return (ToggleState::Value)paramIter.value().toInt();
}
