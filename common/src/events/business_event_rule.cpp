#include "business_event_rule.h"

#include "recording_business_action.h"
#include "sendmail_business_action.h"
#include "common_business_action.h"
#include "camera_output_business_action.h"

#include <core/resource/resource.h>


QnBusinessEventRule::QnBusinessEventRule()
:
    m_eventType( BusinessEventType::BE_NotDefined ),
    m_actionType( BusinessActionType::BA_NotDefined )
{
    //by default, rule triggers on toggle event start. for example: if motion start/stop, send alert on start only
    setEventToggleState(ToggleState::On);
}

QnAbstractBusinessActionPtr QnBusinessEventRule::instantiateAction(QnAbstractBusinessEventPtr bEvent, ToggleState::Value tState) const {
    QnAbstractBusinessActionPtr result;
    switch (m_actionType)
    {
        case BusinessActionType::BA_CameraOutput:
            result = QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction);
            break;
        case BusinessActionType::BA_Bookmark:
        case BusinessActionType::BA_PanicRecording:
            result = QnAbstractBusinessActionPtr(new CommonBusinessAction(m_actionType));
            break;
        case BusinessActionType::BA_CameraRecording:
            result = QnAbstractBusinessActionPtr(new QnRecordingBusinessAction);
            break;
        case BusinessActionType::BA_SendMail:
            result = QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(bEvent->getEventType(),
                                                                              bEvent->getResource(),
                                                                              bEvent->toString()));
            break;
        case BusinessActionType::BA_Alert:
        case BusinessActionType::BA_ShowPopup:
        default:
            return QnAbstractBusinessActionPtr(); // not implemented
    }

    if (!m_actionParams.isEmpty())
        result->setParams(m_actionParams);
    result->setResource(m_actionResource);

    if (BusinessEventType::hasToggleState(bEvent->getEventType()) && BusinessActionType::hasToggleState(m_actionType)) {
        ToggleState::Value value = tState != ToggleState::NotDefined ? tState : bEvent->getToggleState();
        result->setToggleState(value);
    }
    result->setBusinessRuleId(getId());

    return result;
}

BusinessEventType::Value QnBusinessEventRule::eventType() const {
    return m_eventType;
}

void QnBusinessEventRule::setEventType(BusinessEventType::Value value) {
    m_eventType = value;
}


QnResourcePtr QnBusinessEventRule::eventResource() const {
    return m_eventResource;
}

void QnBusinessEventRule::setEventResource(QnResourcePtr value) {
    m_eventResource = value;
}

QnBusinessParams QnBusinessEventRule::eventParams() const {
    return m_eventParams;
}

void QnBusinessEventRule::setEventParams(const QnBusinessParams &params)
{
    m_eventParams = params;
}

BusinessActionType::Value QnBusinessEventRule::actionType() const {
    return m_actionType;
}

void QnBusinessEventRule::setActionType(BusinessActionType::Value value) {
    m_actionType = value;
}

QnResourcePtr QnBusinessEventRule::actionResource() const {
    return m_actionResource;
}

void QnBusinessEventRule::setActionResource(QnResourcePtr value) {
    m_actionResource = value;
}

QnBusinessParams QnBusinessEventRule::actionParams() const
{
    return m_actionParams;
}

void QnBusinessEventRule::setActionParams(const QnBusinessParams &params)
{
    m_actionParams = params;
}

QString QnBusinessEventRule::getUniqueId() const
{
    return QString(QLatin1String("QnBusinessEventRule_")) + getId().toString();
}

ToggleState::Value QnBusinessEventRule::getEventToggleState() const {
    return BusinessEventParameters::getToggleState(m_eventParams);
}

void QnBusinessEventRule::setEventToggleState(ToggleState::Value value) {
    BusinessEventParameters::setToggleState(&m_eventParams, value);
}
