#include "business_event_rule.h"

#include <business/business_action_factory.h>

#include <core/resource/resource.h>
#include "core/resource_management/resource_pool.h"

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


QVector<QnId> QnBusinessEventRule::eventResources() const {
    return m_eventResources;
}

QnResourceList QnBusinessEventRule::eventResourceObjects() const 
{
    QnResourceList result;
    foreach(const QnId& id, m_eventResources) {
        QnResourcePtr res = qnResPool->getResourceById(id);
        if (res)
            result << res;
    }
    return result;
}

void QnBusinessEventRule::setEventResources(const QVector<QnId> &value) {
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

QVector<QnId> QnBusinessEventRule::actionResources() const {
    return m_actionResources;
}

QnResourceList QnBusinessEventRule::actionResourceObjects() const 
{
    QnResourceList result;
    foreach(const QnId& id, m_actionResources) {
        QnResourcePtr res = qnResPool->getResourceById(id);
        if (res)
            result << res;
    }
    return result;
}

void QnBusinessEventRule::setActionResources(const QVector<QnId> &value) {
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

QnBusinessEventRule::QnBusinessEventRule(int internalId, int aggregationPeriod, const QByteArray& actionParams, bool isSystem, BusinessActionType::Value bActionType, BusinessEventType::Value bEventType,
                                         QnResourcePtr actionRes)
{
    m_disabled = false;
    m_eventState = Qn::UndefinedState;
    
    m_id = intToGuid(internalId);
    m_aggregationPeriod = aggregationPeriod;
    m_system = isSystem;
    m_actionType = bActionType;
    m_eventType = bEventType;

    QnBusinessParams bParams = deserializeBusinessParams(actionParams);
    m_actionParams  = QnBusinessActionParameters::fromBusinessParams(bParams);
    if (actionRes)
        m_actionResources << actionRes->getId();
}


QnBusinessEventRule* QnBusinessEventRule::clone()
{
    QnBusinessEventRule* newRule = new QnBusinessEventRule();
    newRule->m_id = m_id;
    newRule->m_eventType = m_eventType;
    newRule->m_eventResources = m_eventResources;
    newRule->m_eventParams = m_eventParams;
    newRule->m_eventState = m_eventState;
    newRule->m_actionType = m_actionType;
    newRule->m_actionResources = m_actionResources;
    newRule->m_actionParams = m_actionParams;
    newRule->m_aggregationPeriod = m_aggregationPeriod;
    newRule->m_disabled = m_disabled;
    newRule->m_schedule = m_schedule;
    newRule->m_binSchedule = m_binSchedule;
    newRule->m_comments = m_comments;
    newRule->m_system = m_system;
    return newRule;
}

void QnBusinessEventRule::removeResource(const QnId& resId)
{
    for (int i = m_actionResources.size() - 1; i >= 0; --i)
    {
        if (m_actionResources[i] == resId)
            m_actionResources.removeAt(i);
    }
    for (int i = m_eventResources.size() - 1; i >= 0; --i)
    {
        if (m_eventResources[i] == resId)
            m_eventResources.removeAt(i);
    }
    
}


QnBusinessEventRuleList QnBusinessEventRule::getDefaultRules()
{
    QnResourcePtr admin = qnResPool->getResourceById(QUuid("e3219e00-cb8f-496c-81a0-28abc1b3a830"));

    QnBusinessEventRuleList result;
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(1,  30,    "{ \"userGroup\" : 0 }",  0, BusinessActionType::ShowPopup,   BusinessEventType::Camera_Disconnect));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(2,  30,    QByteArray(),             0, BusinessActionType::ShowPopup,   BusinessEventType::Storage_Failure));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(3,  30,    QByteArray(),             0, BusinessActionType::ShowPopup,   BusinessEventType::Network_Issue));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(4,  30,    QByteArray(),             0, BusinessActionType::ShowPopup,   BusinessEventType::Camera_Ip_Conflict));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(5,  30,    QByteArray(),             0, BusinessActionType::ShowPopup,   BusinessEventType::MediaServer_Failure));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(6,  30,    QByteArray(),             0, BusinessActionType::ShowPopup,   BusinessEventType::MediaServer_Conflict));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(7,  21600, QByteArray(),             0, BusinessActionType::SendMail,    BusinessEventType::Camera_Disconnect, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(8,  21600, QByteArray(),             0, BusinessActionType::SendMail,    BusinessEventType::Storage_Failure, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(9,  21600, QByteArray(),             0, BusinessActionType::SendMail,    BusinessEventType::Network_Issue, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(10, 21600, QByteArray(),             0, BusinessActionType::SendMail,    BusinessEventType::Camera_Ip_Conflict, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(11, 21600, QByteArray(),             0, BusinessActionType::SendMail,    BusinessEventType::MediaServer_Failure, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(12, 21600, QByteArray(),             0, BusinessActionType::SendMail,    BusinessEventType::MediaServer_Conflict, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(13, 30,    QByteArray(),             1, BusinessActionType::Diagnostics, BusinessEventType::Camera_Disconnect));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(14, 30,    QByteArray(),             1, BusinessActionType::Diagnostics, BusinessEventType::Storage_Failure));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(15, 30,    QByteArray(),             1, BusinessActionType::Diagnostics, BusinessEventType::Network_Issue));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(16, 30,    QByteArray(),             1, BusinessActionType::Diagnostics, BusinessEventType::Camera_Ip_Conflict));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(17, 30,    QByteArray(),             1, BusinessActionType::Diagnostics, BusinessEventType::MediaServer_Failure));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(18, 30,    QByteArray(),             1, BusinessActionType::Diagnostics, BusinessEventType::MediaServer_Conflict));

    return result;
}
