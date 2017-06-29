#include "rule.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <nx/vms/event/action_factory.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

namespace {

static const QList<QnUuid> kOwnerRoleIds {
    QnUserRolesManager::predefinedRoleId(Qn::UserRole::Owner) };

} // namespace

Rule::Rule() :
    m_id(),
    m_eventType(undefinedEvent),
    m_eventState(EventState::active), //< By default, rule triggers on toggle event start.
                               //< For example, if motion starts/stops, send alert on start only.
    m_actionType(undefinedAction),
    m_aggregationPeriod(0),
    m_disabled(false),
    m_system(false)
{
}

Rule::~Rule()
{
}

QnUuid Rule::id() const
{
    return m_id;
}

void Rule::setId(const QnUuid& value)
{
    m_id = value;
}

EventType Rule::eventType() const
{
    return m_eventType;
}

void Rule::setEventType(EventType eventType)
{
    m_eventType = eventType;
}

QVector<QnUuid> Rule::eventResources() const
{
    return m_eventResources;
}

void Rule::setEventResources(const QVector<QnUuid>& value)
{
    m_eventResources = value;
}

EventParameters Rule::eventParams() const
{
    return m_eventParams;
}

void Rule::setEventParams(const EventParameters& params)
{
    m_eventParams = params;
}

EventState Rule::eventState() const
{
    return m_eventState;
}

void Rule::setEventState(EventState state)
{
    m_eventState = state;
}

ActionType Rule::actionType() const
{
    return m_actionType;
}

void Rule::setActionType(ActionType actionType)
{
    m_actionType = actionType;
    //TODO: #GDM #Business fill action params with default values? filter action resources?
}

QVector<QnUuid> Rule::actionResources() const
{
    return m_actionResources;
}

void Rule::setActionResources(const QVector<QnUuid>& value)
{
    m_actionResources = value;
}

ActionParameters Rule::actionParams() const
{
    return m_actionParams;
}

void Rule::setActionParams(const ActionParameters& params)
{
    m_actionParams = params;
}

bool Rule::isActionProlonged() const
{
    return event::isActionProlonged(m_actionType, m_actionParams);
}

int Rule::aggregationPeriod() const
{
    return m_aggregationPeriod;
}

void Rule::setAggregationPeriod(int msecs)
{
    m_aggregationPeriod = msecs;
}

bool Rule::isDisabled() const
{
    return m_disabled;
}

void Rule::setDisabled(bool disabled)
{
    m_disabled = disabled;
}

QString Rule::comment() const
{
    return m_comment;
}

void Rule::setComment(const QString& comment)
{
    m_comment = comment;
}

QString Rule::schedule() const
{
    return m_schedule;
}

void Rule::setSchedule(const QString& schedule)
{
    m_schedule = schedule;
    m_binSchedule = QByteArray::fromHex(m_schedule.toUtf8());
}

bool Rule::isSystem() const
{
    return m_system;
}

void Rule::setSystem(bool system)
{
    m_system = system;
}

QString Rule::getUniqueId() const
{
    return QString(QLatin1String("Rule_%1_")).arg(m_id.toString());
}

bool Rule::isScheduleMatchTime(const QDateTime& datetime) const
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

Rule::Rule(
    int internalId, int aggregationPeriod, bool isSystem, ActionType bActionType,
    EventType bEventType, const QList<QnUuid>& subjectIds, bool allUsers)
{
    m_disabled = false;
    m_eventState = EventState::undefined;

    m_id = intToGuid(internalId, "vms_businessrule");
    m_aggregationPeriod = aggregationPeriod;
    m_system = isSystem;
    m_actionType = bActionType;
    m_eventType = bEventType;

    if (subjectIds.empty())
        return;

    if (requiresUserResource(m_actionType))
        m_actionResources = subjectIds.toVector();
    else
        m_actionParams.additionalResources = subjectIds.toVector().toStdVector();

    m_actionParams.allUsers = allUsers;
}

Rule* Rule::clone()
{
    Rule* newRule = new Rule();
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

void Rule::removeResource(const QnUuid& resId)
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

RuleList Rule::getDefaultRules()
{
    RuleList result;
    result << RulePtr(new Rule(1,  30,      0, showPopupAction,   cameraDisconnectEvent, {}, true));
    result << RulePtr(new Rule(2,  30,      0, showPopupAction,   storageFailureEvent,   {}, true));
    result << RulePtr(new Rule(3,  30,      0, showPopupAction,   networkIssueEvent,     {}, true));
    result << RulePtr(new Rule(4,  30,      0, showPopupAction,   cameraIpConflictEvent, {}, true));
    result << RulePtr(new Rule(5,  30,      0, showPopupAction,   serverFailureEvent,    {}, true));
    result << RulePtr(new Rule(6,  30,      0, showPopupAction,   serverConflictEvent,   {}, true));
    result << RulePtr(new Rule(7,  21600,   0, sendMailAction,    cameraDisconnectEvent, kOwnerRoleIds));
    result << RulePtr(new Rule(8,  24*3600, 0, sendMailAction,    storageFailureEvent,   kOwnerRoleIds));
    result << RulePtr(new Rule(9,  21600,   0, sendMailAction,    networkIssueEvent,     kOwnerRoleIds));
    result << RulePtr(new Rule(10, 21600,   0, sendMailAction,    cameraIpConflictEvent, kOwnerRoleIds));
    result << RulePtr(new Rule(11, 21600,   0, sendMailAction,    serverFailureEvent,    kOwnerRoleIds));
    result << RulePtr(new Rule(12, 21600,   0, sendMailAction,    serverConflictEvent,   kOwnerRoleIds));
    result << RulePtr(new Rule(20, 21600,   0, sendMailAction,    serverStartEvent,      kOwnerRoleIds));

    result << RulePtr(new Rule(22, 21600,   0, sendMailAction,    licenseIssueEvent,     kOwnerRoleIds));
    result << RulePtr(new Rule(23, 30,      0, showPopupAction,   licenseIssueEvent,     {}, true));

    result << getSystemRules() << getRulesUpd43() << getRulesUpd48();
    return result;
}

RuleList Rule::getSystemRules()
{
    return {
        RulePtr(new Rule(900013, 30, 1, diagnosticsAction, cameraDisconnectEvent)),
        RulePtr(new Rule(900014, 30, 1, diagnosticsAction, storageFailureEvent)),
        RulePtr(new Rule(900015, 30, 1, diagnosticsAction, networkIssueEvent)),
        RulePtr(new Rule(900016, 30, 1, diagnosticsAction, cameraIpConflictEvent)),
        RulePtr(new Rule(900017, 30, 1, diagnosticsAction, serverFailureEvent)),
        RulePtr(new Rule(900018, 30, 1, diagnosticsAction, serverConflictEvent)),
        RulePtr(new Rule(900019, 0,  1, diagnosticsAction, serverStartEvent)),
        RulePtr(new Rule(900021, 30, 1, diagnosticsAction, licenseIssueEvent)) };
}

RuleList Rule::getRulesUpd43()
{
    return {
        RulePtr(new Rule(24,     0, 0, showPopupAction,   userDefinedEvent, {}, true)),
        RulePtr(new Rule(900022, 0, 1, diagnosticsAction, userDefinedEvent)) };
}

RuleList Rule::getRulesUpd48()
{
    return {
        RulePtr(new Rule(900023, 0, 0, showPopupAction,   backupFinishedEvent, {}, true)),
        RulePtr(new Rule(900024, 0, 1, diagnosticsAction, backupFinishedEvent)) };
}

} // namespace event
} // namespace vms
} // namespace nx
