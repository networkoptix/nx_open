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
    QnUserRolesManager::predefinedRoleId(Qn::UserRole::owner) };

} // namespace

const QByteArray koldGuidPostfix("DEFAULT_BUSINESS_RULES");
const QByteArray Rule::kGuidPostfix("vms_businessrule");

Rule::Rule() :
    m_id(),
    m_eventType(EventType::undefinedEvent),
    m_eventState(EventState::active), //< By default, rule triggers on toggle event start.
                               //< For example, if motion starts/stops, send alert on start only.
    m_actionType(ActionType::undefinedAction),
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
    // TODO: #GDM #Business fill action params with default values? filter action resources?
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

void Rule::setAggregationPeriod(int seconds)
{
    m_aggregationPeriod = seconds;
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

    m_id = intToGuid(internalId, kGuidPostfix);

    m_aggregationPeriod = aggregationPeriod;
    m_system = isSystem;
    m_actionType = bActionType;
    m_eventType = bEventType;
    m_actionParams.allUsers = allUsers;

    if (!subjectIds.empty())
    {
        if (requiresUserResource(m_actionType))
            m_actionResources = subjectIds.toVector();
        else
            m_actionParams.additionalResources = subjectIds.toVector().toStdVector();
    }
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

QMap<QnUuid, QnUuid> Rule::remappedGuidsToFix()
{
    static const QMap<int, int> intValues =
    {
        { 20, 10020 },
        { 22, 10022 },
        { 23, 10023 }
    };
    QMap<QnUuid, QnUuid> result;
    for (auto itr = intValues.begin(); itr != intValues.end(); ++itr)
    {
        result.insert(
            intToGuid(itr.key(), kGuidPostfix),
            intToGuid(itr.value(), kGuidPostfix));
    }

    for (int i = 0; i <= 19; ++i)
    {
        result.insert(
            intToGuid(i, koldGuidPostfix),
            intToGuid(i, kGuidPostfix));
    }

    for (int i = 10020; i <= 10023; ++i)
    {
        result.insert(
            intToGuid(i, koldGuidPostfix),
            intToGuid(i, kGuidPostfix));
    }

    return result;
}

RuleList Rule::getDefaultRules()
{
    RuleList result;
    //                         Id     period   isSystem  actionType       eventType              subjects   allUsers
    result << RulePtr(new Rule(1,     30,      false, ActionType::showPopupAction, EventType::cameraDisconnectEvent, {},        true));
    result << RulePtr(new Rule(2,     30,      false, ActionType::showPopupAction, EventType::storageFailureEvent,   {},        true));
    result << RulePtr(new Rule(3,     30,      false, ActionType::showPopupAction, EventType::networkIssueEvent,     {},        true));
    result << RulePtr(new Rule(4,     30,      false, ActionType::showPopupAction, EventType::cameraIpConflictEvent, {},        true));
    result << RulePtr(new Rule(5,     30,      false, ActionType::showPopupAction, EventType::serverFailureEvent,    {},        true));
    result << RulePtr(new Rule(6,     30,      false, ActionType::showPopupAction, EventType::serverConflictEvent,   {},        true));
    result << RulePtr(new Rule(7,     21600,   false, ActionType::sendMailAction,  EventType::cameraDisconnectEvent, kOwnerRoleIds));
    result << RulePtr(new Rule(8,     24*3600, false, ActionType::sendMailAction, EventType::storageFailureEvent,   kOwnerRoleIds));
    result << RulePtr(new Rule(9,     21600,   false, ActionType::sendMailAction, EventType::networkIssueEvent,     kOwnerRoleIds));
    result << RulePtr(new Rule(10,    21600,   false, ActionType::sendMailAction, EventType::cameraIpConflictEvent, kOwnerRoleIds));
    result << RulePtr(new Rule(11,    21600,   false, ActionType::sendMailAction, EventType::serverFailureEvent,    kOwnerRoleIds));
    result << RulePtr(new Rule(12,    21600,   false, ActionType::sendMailAction, EventType::serverConflictEvent,   kOwnerRoleIds));
    result << RulePtr(new Rule(10020, 21600,   false, ActionType::sendMailAction, EventType::serverStartEvent,      kOwnerRoleIds));

    result << RulePtr(new Rule(10022, 21600,   false, ActionType::sendMailAction, EventType::licenseIssueEvent,     kOwnerRoleIds));
    result << RulePtr(new Rule(10023, 30,      false, ActionType::showPopupAction, EventType::licenseIssueEvent,     {},        true));

    result
        << getSystemRules()
        << getRulesUpd43()
        << getRulesUpd48()
        << getPluginEventUpdateRules();

    auto disabledRules = getDisabledRulesUpd43();

    result.erase(std::remove_if(result.begin(), result.end(),
        [&disabledRules](const auto& rulePtr)
        {
            return std::any_of(disabledRules.cbegin(), disabledRules.cend(),
                [&rulePtr](const auto& drPtr) { return drPtr->id() == rulePtr->id(); });
        }), result.end());

    return result;
}

RuleList Rule::getSystemRules()
{
    return { //          Id      period isSystem  actionType         eventType
        RulePtr(new Rule(900013, 30,    true, ActionType::diagnosticsAction, EventType::cameraDisconnectEvent)),
        RulePtr(new Rule(900014, 30,    true, ActionType::diagnosticsAction, EventType::storageFailureEvent)),
        RulePtr(new Rule(900015, 30,    true, ActionType::diagnosticsAction, EventType::networkIssueEvent)),
        RulePtr(new Rule(900016, 30,    true, ActionType::diagnosticsAction, EventType::cameraIpConflictEvent)),
        RulePtr(new Rule(900017, 30,    true, ActionType::diagnosticsAction, EventType::serverFailureEvent)),
        RulePtr(new Rule(900018, 30,    true, ActionType::diagnosticsAction, EventType::serverConflictEvent)),
        RulePtr(new Rule(900019, 0,     true, ActionType::diagnosticsAction, EventType::serverStartEvent)),
        RulePtr(new Rule(900021, 30,    true, ActionType::diagnosticsAction, EventType::licenseIssueEvent))
    };
}

RuleList Rule::getRulesUpd43()
{
    return {//           Id      period isSystem actionType         eventType         subjects allUsers
        RulePtr(new Rule(24,     0,     false, ActionType::showPopupAction, EventType::userDefinedEvent, {},      true)),
        RulePtr(new Rule(900022, 0,     true, ActionType::diagnosticsAction, EventType::userDefinedEvent))
    };
}

RuleList Rule::getRulesUpd48()
{
    return {//           Id      period isSystem actionType         eventType            subjects allUsers
        RulePtr(new Rule(900023, 0,     false, ActionType::showPopupAction, EventType::backupFinishedEvent, {},      true)),
        RulePtr(new Rule(900024, 0,     true, ActionType::diagnosticsAction, EventType::backupFinishedEvent))
    };
}

RuleList Rule::getDisabledRulesUpd43()
{
    // Removing extra business rule, that was added through Rule::getRulesUpd43()
    // Required to implement 'Omit db logging' feature.
    return {//           Id      period isSystem actionType         eventType            subjects allUsers
        RulePtr(new Rule(900022, 0,     true,    ActionType::diagnosticsAction, EventType::userDefinedEvent))
    };
}

RuleList Rule::getPluginEventUpdateRules()
{
    using namespace nx::vms::api;
    RulePtr rule(new Rule(
        /*internalId*/ 900025,
        /*aggregationPeriod*/ 0,
        /*isSystem*/ false,
        ActionType::showPopupAction,
        EventType::pluginEvent,
        /*subjects*/ {},
        /*allUsers*/ true));

    EventParameters parameters;
    parameters.inputPortId = QnLexical::serialized(
        EventLevel::InfoEventLevel
        |EventLevel::WarningEventLevel
        |EventLevel::ErrorEventLevel);

    rule->setEventParams(parameters);
    return {rule};
}

} // namespace event
} // namespace vms
} // namespace nx
