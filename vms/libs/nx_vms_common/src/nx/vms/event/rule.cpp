// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/event/action_factory.h>

namespace nx {
namespace vms {
namespace event {

const QByteArray koldGuidPostfix("DEFAULT_BUSINESS_RULES");
const QByteArray Rule::kGuidPostfix("vms_businessrule");

Rule::Rule() :
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
    // TODO: #sivanov Fill action params with default values? Filter action resources?
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
            m_actionParams.additionalResources = std::vector(subjectIds.begin(), subjectIds.end());
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
    auto result = RuleList()
        << getNotificationRules()
        << getSystemRules()
        << getRulesUpd43()
        << getPluginDiagnosticEventUpdateRules()
        << getServerCertificateErrorRules()
        << getLdapSyncIssueRules();

    std::set<QnUuid> ruleIds;
    for (const auto& rule: result)
        NX_ASSERT(ruleIds.insert(rule->id()).second, "Default rule id conflict: %1", rule->id());

    return result;
}

RuleList Rule::getNotificationRules()
{
    return { //          Id     period isSystem actionType                   eventType
        RulePtr(new Rule(1,     30,      false, ActionType::showPopupAction, EventType::cameraDisconnectEvent, {},        true)),
        RulePtr(new Rule(2,     30,      false, ActionType::showPopupAction, EventType::storageFailureEvent,   {},        true)),
        RulePtr(new Rule(3,     30,      false, ActionType::showPopupAction, EventType::networkIssueEvent,     {},        true)),
        RulePtr(new Rule(4,     30,      false, ActionType::showPopupAction, EventType::cameraIpConflictEvent, {},        true)),
        RulePtr(new Rule(5,     30,      false, ActionType::showPopupAction, EventType::serverFailureEvent,    {},        true)),
        RulePtr(new Rule(6,     30,      false, ActionType::showPopupAction, EventType::serverConflictEvent,   {},        true)),
        RulePtr(new Rule(10023, 30,      false, ActionType::showPopupAction, EventType::licenseIssueEvent,     {},        true)),

        RulePtr(new Rule(7,     6*3600, false, ActionType::sendMailAction, EventType::cameraDisconnectEvent, {api::kAdministratorsGroupId})),
        RulePtr(new Rule(8,    24*3600, false, ActionType::sendMailAction, EventType::storageFailureEvent,   {api::kAdministratorsGroupId})),
        RulePtr(new Rule(9,     6*3600, false, ActionType::sendMailAction, EventType::networkIssueEvent,     {api::kAdministratorsGroupId})),
        RulePtr(new Rule(10,    6*3600, false, ActionType::sendMailAction, EventType::cameraIpConflictEvent, {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11,    6*3600, false, ActionType::sendMailAction, EventType::serverFailureEvent,    {api::kAdministratorsGroupId})),
        RulePtr(new Rule(12,    6*3600, false, ActionType::sendMailAction, EventType::serverConflictEvent,   {api::kAdministratorsGroupId})),
        RulePtr(new Rule(10020, 6*3600, false, ActionType::sendMailAction, EventType::serverStartEvent,      {api::kAdministratorsGroupId})),
        RulePtr(new Rule(10022, 6*3600, false, ActionType::sendMailAction, EventType::licenseIssueEvent,     {api::kAdministratorsGroupId})),

        RulePtr(new Rule(11001, 6*3600,  false, ActionType::pushNotificationAction, EventType::cameraDisconnectEvent, {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11002, 24*3600, false, ActionType::pushNotificationAction, EventType::storageFailureEvent,   {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11003, 6*3600,  false, ActionType::pushNotificationAction, EventType::networkIssueEvent,     {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11004, 6*3600,  false, ActionType::pushNotificationAction, EventType::cameraIpConflictEvent, {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11005, 6*3600,  false, ActionType::pushNotificationAction, EventType::serverFailureEvent,    {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11006, 6*3600,  false, ActionType::pushNotificationAction, EventType::serverConflictEvent,   {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11007, 6*3600,  false, ActionType::pushNotificationAction, EventType::serverStartEvent,      {api::kAdministratorsGroupId})),
        RulePtr(new Rule(11008, 6*3600,  false, ActionType::pushNotificationAction, EventType::licenseIssueEvent,     {api::kAdministratorsGroupId})),
    };
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

RuleList Rule::getPluginDiagnosticEventUpdateRules()
{
    using namespace nx::vms::api;
    RulePtr rule(new Rule(
        /*internalId*/ 900025,
        /*aggregationPeriod*/ 0,
        /*isSystem*/ false,
        ActionType::showPopupAction,
        EventType::pluginDiagnosticEvent,
        /*subjects*/ {},
        /*allUsers*/ true));

    EventParameters parameters;
    parameters.inputPortId = QString::fromStdString(nx::reflect::toString(
        EventLevel::info | EventLevel::warning | EventLevel::error));

    rule->setEventParams(parameters);
    return {rule};
}

RuleList Rule::getServerCertificateErrorRules()
{
    return {
        RulePtr(new Rule(
            /*internalId*/ 10024,
            /*aggregationPeriod*/ 30,
            /*isSystem*/ false,
            ActionType::showPopupAction,
            EventType::serverCertificateError,
            /*subjectIds*/ {},
            /*allUsers*/ true)),
        RulePtr(new Rule(
            /*internalId*/ 10025,
            /*aggregationPeriod*/ 6 * 3600,
            /*isSystem*/ false,
            ActionType::sendMailAction,
            EventType::serverCertificateError,
            {api::kAdministratorsGroupId})),
        RulePtr(new Rule(
            /*internalId*/ 10026,
            /*aggregationPeriod*/ 6 * 3600,
            /*isSystem*/ false,
            ActionType::pushNotificationAction,
            EventType::serverCertificateError,
            {api::kAdministratorsGroupId})),
        RulePtr(new Rule(
            /*internalId*/ 900026,
            /*aggregationPeriod*/ 30,
            /*isSystem*/ true,
            ActionType::diagnosticsAction,
            EventType::serverCertificateError)),
    };
}

RuleList Rule::getLdapSyncIssueRules()
{
    return {
        RulePtr(new Rule(
            /*internalId*/ 10027,
            /*aggregationPeriod*/ 6 * 3600,
            /*isSystem*/ false,
            ActionType::sendMailAction,
            EventType::ldapSyncIssueEvent,
            {api::kAdministratorsGroupId})),
        RulePtr(new Rule(
            /*internalId*/ 900027,
            /*aggregationPeriod*/ 30,
            /*isSystem*/ true,
            ActionType::diagnosticsAction,
            EventType::ldapSyncIssueEvent)),
    };
}

} // namespace event
} // namespace vms
} // namespace nx
