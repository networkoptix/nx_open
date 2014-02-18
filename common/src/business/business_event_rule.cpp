#include "business_event_rule.h"

#include <business/business_action_factory.h>

#include <core/resource/resource.h>

QnBusinessEventRule::QnBusinessEventRule()
:
    m_id(),
    m_eventType(BusinessEventType::NotDefined),
    m_eventState(Qn::OnState), //by default, rule triggers on toggle event start. for example: if motion start/stop, send alert on start only
    m_actionType(BusinessActionType::NotDefined),
    m_aggregationPeriod(0),
    m_disabled(false),
    m_system(false)
{
}

QnBusinessEventRule::~QnBusinessEventRule() {

}

QnId QnBusinessEventRule::id() const {
    return m_id;
}

void QnBusinessEventRule::setId(const QnId& value) {
    m_id = value;
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

QnBusinessEventParameters QnBusinessEventRule::eventParams() const {
    return m_eventParams;
}

void QnBusinessEventRule::setEventParams(const QnBusinessEventParameters &params)
{
    m_eventParams = params;
}

Qn::ToggleState QnBusinessEventRule::eventState() const {
    return m_eventState;
}

void QnBusinessEventRule::setEventState(Qn::ToggleState state) {
    m_eventState = state;
}

BusinessActionType::Value QnBusinessEventRule::actionType() const {
    return m_actionType;
}

void QnBusinessEventRule::setActionType(const BusinessActionType::Value value) {
    m_actionType = value;
    //TODO: #GDM fill action params with default values? filter action resources?
}

QnResourceList QnBusinessEventRule::actionResources() const {
    return m_actionResources;
}

void QnBusinessEventRule::setActionResources(const QnResourceList &value) {
    m_actionResources = value;
}

QnBusinessActionParameters QnBusinessEventRule::actionParams() const
{
    return m_actionParams;
}

void QnBusinessEventRule::setActionParams(const QnBusinessActionParameters &params)
{
    m_actionParams = params;
}

int QnBusinessEventRule::aggregationPeriod() const {
    return m_aggregationPeriod;
}

void QnBusinessEventRule::setAggregationPeriod(int msecs) {
    m_aggregationPeriod = msecs;
}

bool QnBusinessEventRule::disabled() const {
    return m_disabled;
}

void QnBusinessEventRule::setDisabled(bool value) {
    m_disabled = value;
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

bool QnBusinessEventRule::system() const {
    return m_system;
}

void QnBusinessEventRule::setSystem(bool value) {
    m_system = value;
}

QString QnBusinessEventRule::getUniqueId() const
{
    return QString(QLatin1String("QnBusinessEventRule_%1_")).arg(m_id.toString());
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
