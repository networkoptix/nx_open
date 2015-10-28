#include "business_event_rule.h"

#include <business/business_action_factory.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/model_functions.h>

QnBusinessEventRule::QnBusinessEventRule()
:
    m_id(),
    m_eventType(QnBusiness::UndefinedEvent),
    m_eventState(QnBusiness::ActiveState), //by default, rule triggers on toggle event start. for example: if motion start/stop, send alert on start only
    m_actionType(QnBusiness::UndefinedAction),
    m_aggregationPeriod(0),
    m_disabled(false),
    m_system(false)
{
}

QnBusinessEventRule::~QnBusinessEventRule() {

}

QnUuid QnBusinessEventRule::id() const {
    return m_id;
}

void QnBusinessEventRule::setId(const QnUuid& value) {
    m_id = value;
}

QnBusiness::EventType QnBusinessEventRule::eventType() const {
    return m_eventType;
}

void QnBusinessEventRule::setEventType(QnBusiness::EventType eventType) {
    m_eventType = eventType;
}


QVector<QnUuid> QnBusinessEventRule::eventResources() const {
    return m_eventResources;
}

void QnBusinessEventRule::setEventResources(const QVector<QnUuid> &value) {
    m_eventResources = value;
}

QnBusinessEventParameters QnBusinessEventRule::eventParams() const {
    return m_eventParams;
}

void QnBusinessEventRule::setEventParams(const QnBusinessEventParameters &params)
{
    m_eventParams = params;
}

QnBusiness::EventState QnBusinessEventRule::eventState() const {
    return m_eventState;
}

void QnBusinessEventRule::setEventState(QnBusiness::EventState state) {
    m_eventState = state;
}

QnBusiness::ActionType QnBusinessEventRule::actionType() const {
    return m_actionType;
}

void QnBusinessEventRule::setActionType(QnBusiness::ActionType actionType) {
    m_actionType = actionType;
    //TODO: #GDM #Business fill action params with default values? filter action resources?
}

QVector<QnUuid> QnBusinessEventRule::actionResources() const {
    return m_actionResources;
}

void QnBusinessEventRule::setActionResources(const QVector<QnUuid> &value) {
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

bool QnBusinessEventRule::isActionProlonged() const {
    return QnBusiness::isActionProlonged(m_actionType, m_actionParams);
}

int QnBusinessEventRule::aggregationPeriod() const {
    return m_aggregationPeriod;
}

void QnBusinessEventRule::setAggregationPeriod(int msecs) {
    m_aggregationPeriod = msecs;
}

bool QnBusinessEventRule::isDisabled() const {
    return m_disabled;
}

void QnBusinessEventRule::setDisabled(bool disabled) {
    m_disabled = disabled;
}

QString QnBusinessEventRule::comment() const {
    return m_comment;
}

void QnBusinessEventRule::setComment(const QString &comment) {
    m_comment = comment;
}

QString QnBusinessEventRule::schedule() const {
    return m_schedule;
}

void QnBusinessEventRule::setSchedule(const QString &schedule) {
    m_schedule = schedule;
    m_binSchedule = QByteArray::fromHex(m_schedule.toUtf8());
}

bool QnBusinessEventRule::isSystem() const {
    return m_system;
}

void QnBusinessEventRule::setSystem(bool system) {
    m_system = system;
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

QnBusinessEventRule::QnBusinessEventRule(
    int internalId, int aggregationPeriod, const QByteArray& actionParams, bool isSystem, 
    QnBusiness::ActionType bActionType, QnBusiness::EventType bEventType, const QnResourcePtr& actionRes)
{
    m_disabled = false;
    m_eventState = QnBusiness::UndefinedState;
    
    m_id = intToGuid(internalId, "vms_businessrule");
    m_aggregationPeriod = aggregationPeriod;
    m_system = isSystem;
    m_actionType = bActionType;
    m_eventType = bEventType;

    m_actionParams = QJson::deserialized<QnBusinessActionParameters>(actionParams);
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
    newRule->m_comment = m_comment;
    newRule->m_system = m_system;
    return newRule;
}

void QnBusinessEventRule::removeResource(const QnUuid& resId)
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
    QnResourcePtr admin = qnResPool->getResourceById(QnUuid("99cbc715-539b-4bfe-856f-799b45b69b1e"));
    Q_ASSERT(admin);
    QnBusinessEventRuleList result;
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(1,  30,    "{ \"userGroup\" : 0 }",  0, QnBusiness::ShowPopupAction,   QnBusiness::CameraDisconnectEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(2,  30,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::StorageFailureEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(3,  30,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::NetworkIssueEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(4,  30,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::CameraIpConflictEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(5,  30,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::ServerFailureEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(6,  30,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::ServerConflictEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(7,  21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::CameraDisconnectEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(8,  21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::StorageFailureEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(9,  21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::NetworkIssueEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(10, 21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::CameraIpConflictEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(11, 21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::ServerFailureEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(12, 21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::ServerConflictEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(20, 21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::ServerStartEvent, admin));
    
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(22, 21600, QByteArray(),             0, QnBusiness::SendMailAction,    QnBusiness::LicenseIssueEvent, admin));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(23, 30,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::LicenseIssueEvent));

    result << getSystemRules() << getRulesUpd43();
    return result;
}

QnBusinessEventRuleList QnBusinessEventRule::getSystemRules()
{
    QnBusinessEventRuleList result;
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900013, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::CameraDisconnectEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900014, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::StorageFailureEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900015, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::NetworkIssueEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900016, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::CameraIpConflictEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900017, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::ServerFailureEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900018, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::ServerConflictEvent));

    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900019, 0,     QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::ServerStartEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900021, 30,    QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::LicenseIssueEvent));

    return result;
}

QnBusinessEventRuleList QnBusinessEventRule::getRulesUpd43()
{
    QnBusinessEventRuleList result;
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(24,  0,    QByteArray(),             0, QnBusiness::ShowPopupAction,   QnBusiness::UserDefinedEvent));
    result << QnBusinessEventRulePtr(new QnBusinessEventRule(900022, 0,     QByteArray(),             1, QnBusiness::DiagnosticsAction, QnBusiness::UserDefinedEvent));
    return result;
}
