#include "business_event_rule.h"

#include "recording_business_action.h"
#include "sendmail_business_action.h"
#include "common_business_action.h"
#include "camera_output_business_action.h"

#include <events/business_action_factory.h>

#include <core/resource/resource.h>


QnBusinessEventRule::QnBusinessEventRule()
:
    m_eventType(BusinessEventType::BE_NotDefined),
    m_eventState(ToggleState::On), //by default, rule triggers on toggle event start. for example: if motion start/stop, send alert on start only
    m_actionType(BusinessActionType::BA_NotDefined),
    m_aggregationPeriod(0)
{
}

QnAbstractBusinessActionPtr QnBusinessEventRule::instantiateAction(QnAbstractBusinessEventPtr bEvent, ToggleState::Value tState) const {
    if (BusinessActionType::isResourceRequired(m_actionType) && m_actionResources.isEmpty())
        return QnAbstractBusinessActionPtr(); //resource is not exists anymore
    //TODO: #GDM check resource type?

    QnBusinessParams runtimeParams = bEvent->getRuntimeParams();

    QnAbstractBusinessActionPtr result = QnBusinessActionFactory::createAction(m_actionType, runtimeParams);

    if (!m_actionParams.isEmpty())
        result->setParams(m_actionParams);
    result->setResources(m_actionResources);

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

void QnBusinessEventRule::setEventType(const BusinessEventType::Value value) {
    m_eventType = value;
}


QnResourceList QnBusinessEventRule::eventResources() const {
    return m_eventResources;
}

void QnBusinessEventRule::setEventResources(const QnResourceList &value) {
    m_eventResources = value;
}

QnBusinessParams QnBusinessEventRule::eventParams() const {
    return m_eventParams;
}

void QnBusinessEventRule::setEventParams(const QnBusinessParams &params)
{
    m_eventParams = params;
}

ToggleState::Value QnBusinessEventRule::eventState() const {
    return m_eventState;
}

void QnBusinessEventRule::setEventState(ToggleState::Value state) {
    m_eventState = state;
}

BusinessActionType::Value QnBusinessEventRule::actionType() const {
    return m_actionType;
}

void QnBusinessEventRule::setActionType(const BusinessActionType::Value value) {
    m_actionType = value;
}

QnResourceList QnBusinessEventRule::actionResources() const {
    return m_actionResources;
}

void QnBusinessEventRule::setActionResources(const QnResourceList &value) {
    m_actionResources = value;
}

QnBusinessParams QnBusinessEventRule::actionParams() const
{
    return m_actionParams;
}

void QnBusinessEventRule::setActionParams(const QnBusinessParams &params)
{
    m_actionParams = params;
}

int QnBusinessEventRule::aggregationPeriod() const {
    return m_aggregationPeriod;
}

void QnBusinessEventRule::setAggregationPeriod(int msecs) {
    m_aggregationPeriod = msecs;
}

QString QnBusinessEventRule::comments() const {
    return m_comments;
}

void QnBusinessEventRule::setComments(const QString value) {
    m_comments = value;
}

QString QnBusinessEventRule::schedule() const {
    return m_schedule;
}

void QnBusinessEventRule::setSchedule(const QString value) {
    m_schedule = value;
    m_binSchedule = QByteArray::fromHex(m_schedule.toUtf8());
}

QString QnBusinessEventRule::getUniqueId() const
{
    return QString(QLatin1String("QnBusinessEventRule_")) + getId().toString();
}

bool QnBusinessEventRule::isScheduleMatchTime(const QDateTime& datetime) const
{
    if (m_binSchedule.isEmpty())
        return true; // if no schedule at all do not filter anything

    int currentWeekHour = (datetime.date().dayOfWeek()-1)*24 + datetime.time().hour();

    int byteOffset = currentWeekHour/8;
    if (byteOffset >= m_binSchedule.size())
        return false;
    int bitNum = 7 - (currentWeekHour % 8);
    quint8 mask = 1 << bitNum;

    bool rez = bool(m_binSchedule.at(byteOffset) & mask);
    return rez;
}
