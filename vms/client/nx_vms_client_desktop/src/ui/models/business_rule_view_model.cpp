// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_rule_view_model.h"

#include <QtGui/QStandardItemModel>

#include <business/business_resource_validation.h> //< TODO: #vkutin Move these to proper locations and namespaces
#include <business/business_types_comparator.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/rules/event_action_subtype.h>
#include <nx/vms/client/desktop/rules/helpers/exit_fullscreen_action_helper.h>
#include <nx/vms/client/desktop/rules/helpers/fullscreen_action_helper.h>
#include <nx/vms/client/desktop/rules/nvr_events_actions_access.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/actions/actions.h>
#include <nx/vms/event/events/events.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/text/human_readable.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::api;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::event;

namespace {

const int ProlongedActionRole = Qt::UserRole + 2;
const int defaultActionDurationMs = 5000;
const int defaultAggregationPeriodSec = 60;

QString braced(const QString& source)
{
    return '<' + source + '>';
};

QVector<QnUuid> toIdList(const QSet<QnUuid>& src)
{
    return QVector<QnUuid>(src.begin(), src.end());
}

QSet<QnUuid> toIds(const QnResourceList& resources)
{
    return nx::utils::toQSet(resources.ids());
}

QSet<QnUuid> filterEventResources(const QSet<QnUuid>& ids, nx::vms::api::EventType eventType)
{
    auto resourcePool = appContext()->currentSystemContext()->resourcePool();

    if (requiresCameraResource(eventType))
        return toIds(resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids));

    if (requiresServerResource(eventType))
        return toIds(resourcePool->getResourcesByIds<QnMediaServerResource>(ids));

    return QSet<QnUuid>();
}

template<class IDList>
QSet<QnUuid> filterSubjectIds(const IDList& ids)
{
    QnUserResourceList users;
    QList<QnUuid> groupIds;

    nx::vms::common::getUsersAndGroups(appContext()->currentSystemContext(),
        ids, users, groupIds);

    return toIds(users).unite(nx::utils::toQSet(groupIds));
}

QSet<QnUuid> filterActionResources(
    const QnBusinessRuleViewModel* model,
    const QSet<QnUuid>& ids,
    ActionType actionType)
{
    auto resourcePool = model->resourcePool();

    if (actionType == ActionType::fullscreenCameraAction)
    {
        return toIds(resourcePool->getResourcesByIds<QnLayoutResource>(ids))
            | toIds(resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids));
    }
    else if (actionType == ActionType::exitFullscreenAction)
    {
        return toIds(resourcePool->getResourcesByIds<QnLayoutResource>(ids));
    }

    if (requiresCameraResource(actionType))
        return toIds(resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids));

    if (requiresUserResource(actionType))
        return filterSubjectIds(ids);

    if (requiresServerResource(actionType))
        return toIds(resourcePool->getResourcesByIds<QnMediaServerResource>(ids));

    return QSet<QnUuid>();
}

/**
*  This method must cleanup all action parameters that are not required for the given action.
*  // TODO: #sivanov Implement correct filtering.
*/
ActionParameters filterActionParams(ActionType actionType, const ActionParameters &params)
{
    Q_UNUSED(actionType);
    return params;
}

bool eventCanHaveCamera(EventType eventType)
{
    return (eventType >= EventType::userDefinedEvent) || requiresCameraResource(eventType);
}

} // namespace

QList<QnBusinessRuleViewModel::Column> QnBusinessRuleViewModel::allColumns()
{
    static QList<Column> result {
        Column::modified,
        Column::disabled,
        Column::event,
        Column::source,
        Column::spacer,
        Column::action,
        Column::target,
        Column::aggregation };

    return result;
}

const QnUuid QnBusinessRuleViewModel::kAllUsersId;

QnBusinessRuleViewModel::QnBusinessRuleViewModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_id(QnUuid::createUuid()),
    m_modified(false),
    m_eventType(EventType::cameraDisconnectEvent),
    m_eventState(EventState::undefined),
    m_actionType(ActionType::showPopupAction),
    m_aggregationPeriodSec(defaultAggregationPeriodSec),
    m_disabled(false),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this)),
    m_helper(new StringsHelper(systemContext()))
{
    const auto addEventItem =
        [this](EventType eventType)
        {
            auto item = new QStandardItem(m_helper->eventName(eventType));
            item->setData(eventType);
            m_eventTypesModel->appendRow(item);
        };

    const auto addActionItem =
        [this](ActionType actionType)
        {
            auto item = new QStandardItem(m_helper->actionName(actionType));
            item->setData(actionType);
            item->setData(!canBeInstant(actionType), ProlongedActionRole);
            m_actionTypesModel->appendRow(item);
        };

    const auto addSeparator =
        [](QStandardItemModel* model)
        {
            auto item = new QStandardItem(lit("-"));
            item->setData(lit("separator"), Qt::AccessibleDescriptionRole);
            model->appendRow(item);
        };

    const auto accessibleEvents = NvrEventsActionsAccess::removeInacessibleNvrEvents(
        allEvents(), resourcePool());

    QnBusinessTypesComparator lexComparator;

    const auto userEvents = filterEventsBySubtype(accessibleEvents, EventSubtype::user);
    const auto failureEvents = filterEventsBySubtype(accessibleEvents, EventSubtype::failure);
    const auto successEvents = filterEventsBySubtype(accessibleEvents, EventSubtype::success);

    for (const auto eventType: lexComparator.lexSortedEvents(userEvents))
    {
        if (eventType == EventType::pluginDiagnosticEvent)
        {
            /* Show it only for users who have at least one plugin installed. */
            const auto pirs =
                resourcePool()->getResources<nx::vms::common::AnalyticsEngineResource>();
            if (pirs.isEmpty())
                continue;
        }
        addEventItem(eventType);
    }
    addSeparator(m_eventTypesModel);
    for (const auto eventType: lexComparator.lexSortedEvents(failureEvents))
        addEventItem(eventType);
    addSeparator(m_eventTypesModel);
    for (const auto eventType: lexComparator.lexSortedEvents(successEvents))
        addEventItem(eventType);

    const auto accessibleActions = NvrEventsActionsAccess::removeInacessibleNvrActions(
        userAvailableActions(), resourcePool());

    const auto serverActions = filterActionsBySubtype(accessibleActions, ActionSubtype::server);
    const auto clientActions = filterActionsBySubtype(accessibleActions, ActionSubtype::client);

    for (const auto actionType: lexComparator.lexSortedActions(serverActions))
        addActionItem(actionType);
    addSeparator(m_actionTypesModel);
    for (const auto actionType: lexComparator.lexSortedActions(clientActions))
        addActionItem(actionType);

    m_actionParams.additionalResources = nx::toStdVector(nx::vms::api::kAllPowerUserGroupIds);

    updateActionTypesModel();
    updateEventStateModel();
}

QnBusinessRuleViewModel::~QnBusinessRuleViewModel()
{
}

QVariant QnBusinessRuleViewModel::data(Column column, const int role) const
{
    if (column == Column::disabled)
    {
        switch (role)
        {
            case Qt::CheckStateRole:
                return (m_disabled ? Qt::Unchecked : Qt::Checked);

            case Qt::ToolTipRole:
            case Qt::StatusTipRole:
            case Qt::WhatsThisRole:
            case Qt::AccessibleDescriptionRole:
                return getToolTip(column);

            default:
                break;
        }
    }

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return getText(column);

        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleDescriptionRole:
            return getToolTip(column);

        case Qt::DecorationRole:
            return getIcon(column);

        case Qt::EditRole:
            switch (column)
            {
                case Column::event:
                    return m_eventType;
                case Column::action:
                    return m_actionType;
                case Column::target:
                    if (m_actionType == ActionType::sendMailAction)
                        return m_actionParams.emailAddress;
                    break;
                case Column::aggregation:
                    return m_aggregationPeriodSec;
                default:
                    break;
            }
            return QVariant();
        case core::UuidRole:
            return QVariant::fromValue(m_id);
        case Qn::ModifiedRole:
            return m_modified;
        case Qn::DisabledRole:
            return m_disabled;
        case Qn::ValidRole:
            return isValid();
        case Qn::ActionIsInstantRole:
            return !isActionProlonged();
        case Qn::ShortTextRole:
            return getText(column, false);

        case Qn::EventTypeRole:
            return QVariant::fromValue(m_eventType);
        case Qn::EventParametersRole:
            return QVariant::fromValue(m_eventParams);
        case Qn::EventResourcesRole:
            return QVariant::fromValue(filterEventResources(m_eventResources, m_eventType));
        case Qn::ActionTypeRole:
            return QVariant::fromValue(m_actionType);
        case Qn::ActionResourcesRole:
        {
            auto ids = (
                m_actionType != ActionType::showPopupAction
                && m_actionType != ActionType::pushNotificationAction
                && m_actionType != ActionType::openLayoutAction)
                    ? filterActionResources(this, m_actionResources, m_actionType)
                    : filterSubjectIds(m_actionParams.additionalResources);

            if (m_actionParams.allUsers)
                ids.insert(kAllUsersId);

            return QVariant::fromValue(ids);
        }
        case Qn::HelpTopicIdRole:
            return getHelpTopic(column);
        default:
            break;
    }
    return QVariant();
}

bool QnBusinessRuleViewModel::setData(Column column, const QVariant& value, int role)
{
    if (column == Column::disabled && role == Qt::CheckStateRole)
    {
        setDisabled(value.toInt() == Qt::Unchecked);
        return true;
    }

    if (role != Qt::EditRole)
        return false;

    switch (column)
    {
        case Column::event:
            setEventType((EventType)value.toInt());
            return true;

        case Column::action:
            setActionType((ActionType)value.toInt());
            return true;

        case Column::source:
            setEventResources(value.value<QSet<QnUuid>>());
            return true;

        case Column::target:
        {
            auto subjects = value.value<QSet<QnUuid>>();
            const bool allUsers = subjects.remove(kAllUsersId);

            if (allUsers != m_actionParams.allUsers)
            {
                auto params = m_actionParams;
                params.allUsers = allUsers;
                setActionParams(params);
                if (allUsers)
                    return true;
            }

            switch (m_actionType)
            {
                case ActionType::openLayoutAction:
                case ActionType::showPopupAction:
                case ActionType::pushNotificationAction:
                {
                    auto params = m_actionParams;
                    params.additionalResources = decltype(params.additionalResources)(
                        subjects.cbegin(), subjects.cend());
                    setActionParams(params);
                    break;
                }
                default:
                    setActionResources(subjects);
                    break;
            }
            return true;
        }

        case Column::aggregation:
            setAggregationPeriod(value.toInt());
            return true;

        default:
            break;
    }

    return false;
}


void QnBusinessRuleViewModel::loadFromRule(const RulePtr& businessRule)
{
    m_id = businessRule->id();
    m_modified = false;
    if (m_eventType != businessRule->eventType())
    {
        m_eventType = businessRule->eventType();
        updateEventStateModel();
    }

    m_eventResources =
        filterEventResources(nx::utils::toQSet(businessRule->eventResources()), m_eventType);

    m_eventParams = businessRule->eventParams();

    m_eventState = businessRule->eventState();

    m_actionType = businessRule->actionType();

    m_actionResources = nx::utils::toQSet(businessRule->actionResources());

    m_actionParams = businessRule->actionParams();

    m_aggregationPeriodSec = businessRule->aggregationPeriod();

    m_disabled = businessRule->isDisabled();
    m_comments = businessRule->comment();
    m_schedule = businessRule->schedule();

    updateActionTypesModel(); //< TODO: #sivanov Connect on dataChanged?

    emit dataChanged(Field::all);
}

RulePtr QnBusinessRuleViewModel::createRule() const
{
    RulePtr rule(new Rule());
    rule->setId(m_id);
    rule->setEventType(m_eventType);
    rule->setEventResources(toIdList(filterEventResources(m_eventResources, m_eventType)));
    rule->setEventState(m_eventState);   //< TODO: #sivanov Check this.
    rule->setEventParams(m_eventParams); //< TODO: #sivanov Filter params.
    rule->setActionType(m_actionType);
    rule->setActionResources(toIdList(filterActionResources(this, m_actionResources, m_actionType)));
    rule->setActionParams(filterActionParams(m_actionType, m_actionParams));
    rule->setAggregationPeriod(m_aggregationPeriodSec);
    rule->setDisabled(m_disabled);
    rule->setComment(m_comments);
    rule->setSchedule(m_schedule);
    return rule;
}

// setters and getters


QnUuid QnBusinessRuleViewModel::id() const
{
    return m_id;
}

bool QnBusinessRuleViewModel::isModified() const
{
    return m_modified;
}

void QnBusinessRuleViewModel::setModified(bool value)
{
    if (m_modified == value)
        return;

    m_modified = value;
    emit dataChanged(Field::modified);
}

EventType QnBusinessRuleViewModel::eventType() const
{
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const EventType value)
{
    if (m_eventType == value)
        return;

    bool cameraRequired = requiresCameraResource(m_eventType);
    bool serverRequired = requiresServerResource(m_eventType);

    // Store params for the current event type
    m_cachedEventParams[m_eventType] = m_eventParams;

    m_eventType = value;
    m_modified = true;

    // Create or load params for the new event type
    if (!m_cachedEventParams.contains(m_eventType))
    {
        EventParameters eventParams;

        switch (m_eventType)
        {
            case EventType::pluginDiagnosticEvent:
            {
                using namespace nx::vms::api;
                const EventLevels levels =
                    EventLevel::error | EventLevel::warning | EventLevel::info;
                eventParams.inputPortId = QString::fromStdString(nx::reflect::toString(levels));
                break;
            }

            case EventType::softwareTriggerEvent:
                eventParams.inputPortId = QnUuid::createUuid().toSimpleString();
                eventParams.description = SoftwareTriggerPixmaps::defaultPixmapName();
                break;

            default:
                break;
        }

        m_cachedEventParams[m_eventType] = eventParams;
    }
    m_eventParams = m_cachedEventParams[m_eventType];

    Fields fields = Field::eventType | Field::modified;

    if (requiresCameraResource(m_eventType) != cameraRequired ||
        requiresServerResource(m_eventType) != serverRequired)
    {
        fields |= Field::eventResources;
    }

    fields |= updateEventClassRelatedParams();

    emit dataChanged(fields);
    // TODO: #sivanov Check others, params and resources should be merged.
}

QnBusinessRuleViewModel::Fields QnBusinessRuleViewModel::updateEventClassRelatedParams()
{
    Fields fields = Field::modified;
    if (hasToggleState(m_eventType, m_eventParams, systemContext()))
    {
        if (!isActionProlonged())
        {
            const auto allowedStates =
                allowedEventStates(m_eventType, m_eventParams, systemContext());
            if (!allowedStates.contains(m_eventState))
            {
                m_eventState = allowedStates.first();
                fields |= Field::eventState;
            }
        }
    }
    else
    {
        if (m_eventState != EventState::undefined)
        {
            m_eventState = EventState::undefined;
            fields |= Field::eventState;
        }

        if (!canBeInstant(m_actionType))
        {
            m_actionType = ActionType::showPopupAction;
            fields |= Field::actionType | Field::actionResources | Field::actionParams;
        }
        else if (isActionProlonged() && supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
        {
            m_actionParams.durationMs = defaultActionDurationMs;
            fields |= Field::actionParams;
        }
    }

    updateActionTypesModel();
    updateEventStateModel();

    return fields;
}

QIcon QnBusinessRuleViewModel::iconForAction() const
{
    if (isActionUsingSourceCamera())
        return qnResIconCache->icon(QnResourceIconCache::Camera);

    static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions colorSubs = {
        {QnIcon::Normal, {.primary = "light10"}}, {QnIcon::Selected, {.primary = "light4"}}};

    switch (m_actionType)
    {
        case ActionType::sendMailAction:
        {
            if (!isValid(Column::target))
                return qnSkin->icon("tree/user_alert.svg", colorSubs);
            return qnResIconCache->icon(QnResourceIconCache::Users);
        }

        case ActionType::openLayoutAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        {
            if (m_actionParams.allUsers)
                return qnResIconCache->icon(QnResourceIconCache::Users);
            if (!isValid(Column::target))
                return qnSkin->icon("tree/user_alert.svg", colorSubs);

            QnUserResourceList users;
            QList<QnUuid> groupIds;
            nx::vms::common::getUsersAndGroups(systemContext(),
                m_actionParams.additionalResources, users, groupIds);
            users = users.filtered(
                [](const QnUserResourcePtr& user) { return user->isEnabled(); });
            return (users.size() > 1 || !groupIds.empty())
                ? qnResIconCache->icon(QnResourceIconCache::Users)
                : qnResIconCache->icon(QnResourceIconCache::User);
        }

        case ActionType::fullscreenCameraAction:
        {
            return FullscreenActionHelper::tableCellIcon(this);
        }

        case ActionType::exitFullscreenAction:
        {
            return ExitFullscreenActionHelper::tableCellIcon(this);
        }

        case ActionType::buzzerAction:
        {
            if (actionIsUsingSourceServer())
                return getIcon(Column::source);
        }

        default:
            break;
    }

    auto resourceIcon =
        [this](QnResourceIconCache::Key defaultKey)
        {
            const QnResourceList resources = resourcePool()->getResourcesByIds(actionResources());
            if (resources.empty())
                return qnSkin->icon("tree/buggy.png");

            if (resources.size() == 1)
                return qnResIconCache->icon(resources.first());

            return qnResIconCache->icon(defaultKey);
        };

    if (requiresCameraResource(m_actionType))
        return resourceIcon(QnResourceIconCache::Camera);
    if (requiresServerResource(m_actionType))
        return resourceIcon(QnResourceIconCache::Server);
    if (requiresUserResource(m_actionType))
        return resourceIcon(QnResourceIconCache::User);

    // Icon for the <System> actions.
    return qnResIconCache->icon(QnResourceIconCache::CurrentSystem);
}

QSet<QnUuid> QnBusinessRuleViewModel::eventResources() const
{
    return filterEventResources(m_eventResources, m_eventType);
}

void QnBusinessRuleViewModel::setEventResources(const QSet<QnUuid>& value)
{
    auto filtered = filterEventResources(value, m_eventType);
    auto oldFiltered = filterEventResources(m_eventResources, m_eventType);

    if (filtered == oldFiltered)
        return;

    m_eventResources = value;
    m_modified = true;

    emit dataChanged(Field::eventResources | Field::modified);
}

EventParameters QnBusinessRuleViewModel::eventParams() const
{
    return m_eventParams;
}

void QnBusinessRuleViewModel::setEventParams(const EventParameters &params)
{
    const bool hasChanges = !(m_eventParams == params);

    if (!hasChanges)
        return;

    const bool hasAnalyticsChanges =
        m_eventParams.getAnalyticsEventTypeId() != params.getAnalyticsEventTypeId();
    m_eventParams = params;
    m_modified = true;
    auto fields = Field::eventParams | Field::modified;
    if (hasAnalyticsChanges)
    {
        updateEventClassRelatedParams();
        fields |= Field::eventType;
    }
    emit dataChanged(fields);
}

EventState QnBusinessRuleViewModel::eventState() const
{
    return m_eventState;
}

void QnBusinessRuleViewModel::setEventState(EventState state)
{
    if (m_eventState == state)
        return;

    m_eventState = state;
    m_modified = true;

    emit dataChanged(Field::eventState | Field::modified);
}

ActionType QnBusinessRuleViewModel::actionType() const
{
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const ActionType value)
{
    if (m_actionType == value)
        return;

    const bool cameraWasRequired = requiresCameraResource(m_actionType);
    const bool userWasRequired = requiresUserResource(m_actionType);

    const bool wasEmailAction = m_actionType == ActionType::sendMailAction;
    const bool aggregationWasDisabled = !allowsAggregation(m_actionType);

    // Store params for the current action type
    m_cachedActionParams[m_actionType] = m_actionParams;

    m_actionType = value;
    m_modified = true;

    const bool cameraIsRequired = requiresCameraResource(m_actionType);
    const bool userIsRequired = requiresUserResource(m_actionType);
    const bool additionalUserIsRequired = requiresAdditionalUserResource(m_actionType);

    // Create or load params for the new action type
    if (!m_cachedActionParams.contains(m_actionType))
    {
        ActionParameters actionParams;

        actionParams.allUsers = false;
        actionParams.additionalResources = additionalUserIsRequired
            ? nx::toStdVector(nx::vms::api::kAllPowerUserGroupIds)
            : std::vector<QnUuid>();

        switch (m_actionType)
        {
            case ActionType::pushNotificationAction:
                actionParams.useSource = true; //< Default behaviour.
                break;

            case ActionType::fullscreenCameraAction:
            case ActionType::openLayoutAction:
            case ActionType::showOnAlarmLayoutAction:
                actionParams.recordBeforeMs = 0;
                break;

            default:
                break;
        }

        m_cachedActionParams[m_actionType] = actionParams;
    }
    m_actionParams = m_cachedActionParams[m_actionType];

    if (userIsRequired && !userWasRequired)
        m_actionResources = nx::utils::toQSet(nx::vms::api::kAllPowerUserGroupIds);

    Fields fields = Field::actionType | Field::actionParams | Field::modified;
    if (cameraIsRequired != cameraWasRequired || userIsRequired != userWasRequired)
        fields |= Field::actionResources;

    if (!allowsAggregation(m_actionType))
    {
        m_aggregationPeriodSec = 0;
        fields |= Field::aggregation;
    }
    else
    {
        /*
         *  If action is "send email" default units for aggregation period should be hours, not minutes.
         *  Works only if aggregation period was not changed from default value.
         */
        if (value == ActionType::sendMailAction && m_aggregationPeriodSec == defaultAggregationPeriodSec)
        {
            m_aggregationPeriodSec = defaultAggregationPeriodSec * 60;
            fields |= Field::aggregation;
        }
        else if (wasEmailAction && m_aggregationPeriodSec == defaultAggregationPeriodSec * 60)
        {
            m_aggregationPeriodSec = defaultAggregationPeriodSec;
            fields |= Field::aggregation;
        }
        else if (aggregationWasDisabled)
        {
            m_aggregationPeriodSec = defaultAggregationPeriodSec;
            fields |= Field::aggregation;
        }
    }

    if (hasToggleState(m_eventType, m_eventParams, systemContext()) &&
        !isActionProlonged() && m_eventState == EventState::undefined)
    {
        m_eventState = allowedEventStates(m_eventType, m_eventParams, systemContext()).first();
        fields |= Field::eventState;
    }
    else if (!hasToggleState(m_eventType, m_eventParams, systemContext()) &&
        supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
    {
        m_actionParams.durationMs = defaultActionDurationMs;
        fields |= Field::actionParams;
    }

    emit dataChanged(fields);
}

QSet<QnUuid> QnBusinessRuleViewModel::actionResources() const
{
    return filterActionResources(this, m_actionResources, m_actionType);
}

void QnBusinessRuleViewModel::setActionResources(const QSet<QnUuid>& value)
{
    auto filtered = filterActionResources(this, value, m_actionType);
    auto oldFiltered = filterActionResources(this, m_actionResources, m_actionType);

    if (filtered == oldFiltered)
        return;

    setActionResourcesRaw(value);
}

QSet<QnUuid> QnBusinessRuleViewModel::actionResourcesRaw() const
{
    return m_actionResources;
}

void QnBusinessRuleViewModel::setActionResourcesRaw(const QSet<QnUuid>& value)
{
    m_actionResources = value;
    m_modified = true;

    emit dataChanged(Field::actionResources | Field::modified);
}

bool QnBusinessRuleViewModel::isActionProlonged() const
{
    return ::isActionProlonged(m_actionType, m_actionParams);
}

ActionParameters QnBusinessRuleViewModel::actionParams() const
{
    return m_actionParams;
}

void QnBusinessRuleViewModel::setActionParams(const ActionParameters &params)
{
    if (params == m_actionParams)
        return;

    m_actionParams = params;
    m_modified = true;

    emit dataChanged(Field::actionParams | Field::modified);
}

int QnBusinessRuleViewModel::aggregationPeriod() const
{
    return m_aggregationPeriodSec;
}

void QnBusinessRuleViewModel::setAggregationPeriod(int seconds)
{
    if (m_aggregationPeriodSec == seconds)
        return;

    m_aggregationPeriodSec = seconds;
    m_modified = true;

    emit dataChanged(Field::aggregation | Field::modified);
}

bool QnBusinessRuleViewModel::disabled() const
{
    return m_disabled;
}

void QnBusinessRuleViewModel::setDisabled(bool value)
{
    if (m_disabled == value)
        return;

    m_disabled = value;
    m_modified = true;

    emit dataChanged(Field::all); // all fields should be redrawn
}

bool QnBusinessRuleViewModel::actionCanUseSourceCamera() const
{
    return eventCanHaveCamera(m_eventType) && canUseSourceCamera(m_actionType);
}

bool QnBusinessRuleViewModel::isActionUsingSourceCamera() const
{
    return m_actionParams.useSource && actionCanUseSourceCamera();
}

QString QnBusinessRuleViewModel::sourceCameraCheckboxText() const
{
    return m_helper->sourceCameraCheckboxText(m_actionType);
}

bool QnBusinessRuleViewModel::actionCanUseSourceServer() const
{
    return requiresServerResource(m_actionType)
        && requiresServerResource(m_eventType);
}

bool QnBusinessRuleViewModel::actionIsUsingSourceServer() const
{
    return m_actionParams.useSource && actionCanUseSourceServer();
}

void QnBusinessRuleViewModel::toggleActionUseSourceServer()
{
    if (NX_ASSERT(actionCanUseSourceServer()))
        toggleActionUseSource();
}

void QnBusinessRuleViewModel::toggleActionUseSourceCamera()
{
    if (NX_ASSERT(actionCanUseSourceCamera()))
        toggleActionUseSource();
}

void QnBusinessRuleViewModel::toggleActionUseSource()
{
    m_actionParams.useSource = !m_actionParams.useSource;
    m_modified = true;

    emit dataChanged(Field::actionParams | Field::modified);
}

QString QnBusinessRuleViewModel::comments() const
{
    return m_comments;
}

void QnBusinessRuleViewModel::setComments(const QString& value)
{
    if (m_comments == value)
        return;

    m_comments = value;
    m_modified = true;

    emit dataChanged(Field::comments | Field::modified);
}

QString QnBusinessRuleViewModel::schedule() const
{
    return m_schedule;
}

void QnBusinessRuleViewModel::setSchedule(const QString& value)
{
    if (m_schedule == value)
        return;

    m_schedule = value;
    m_modified = true;

    emit dataChanged(Field::schedule | Field::modified);
}

QStandardItemModel* QnBusinessRuleViewModel::eventTypesModel()
{
    return m_eventTypesModel;
}

QStandardItemModel* QnBusinessRuleViewModel::eventStatesModel()
{
    return m_eventStatesModel;
}

QStandardItemModel* QnBusinessRuleViewModel::actionTypesModel()
{
    return m_actionTypesModel;
}

// utilities

QString QnBusinessRuleViewModel::getText(Column column, bool detailed) const
{
    switch (column)
    {
        case Column::modified:
            return (m_modified ? QLatin1String("*") : QString());
        case Column::event:
            return m_helper->eventTypeString(m_eventType, m_eventState, m_actionType, m_actionParams);
        case Column::source:
            return getSourceText(detailed);
        case Column::spacer:
            return QString();
        case Column::action:
            return m_helper->actionName(m_actionType);
        case Column::target:
            return getTargetText(detailed);
        case Column::aggregation:
            return getAggregationText();
        default:
            break;
    }
    return QString();
}

QString QnBusinessRuleViewModel::getToolTip(Column column) const
{
    if (isValid())
        return m_comments.isEmpty() ? getText(column) : m_comments;

    const QString errorMessage = tr("Error: %1");

    switch (column)
    {
        case Column::modified:
        case Column::event:
        case Column::source:
            if (!isValid(Column::source))
                return errorMessage.arg(getText(Column::source));
            return errorMessage.arg(getText(Column::target));
        default:
            // at least one of them should be invalid
            if (!isValid(Column::target))
                return errorMessage.arg(getText(Column::target));
            return errorMessage.arg(getText(Column::source));
    }
    return QString();
}

QIcon QnBusinessRuleViewModel::getIcon(Column column) const
{
    switch (column)
    {
        case Column::source:
        {
            // TODO: #sivanov Check all variants or resource requirements:
            // userResource, serverResource, etc.
            auto resources = resourcePool()->getResourcesByIds(eventResources());
            if (!isResourceRequired(m_eventType))
            {
                return qnResIconCache->icon(QnResourceIconCache::CurrentSystem);
            }
            else if (resources.size() == 1)
            {
                QnResourcePtr resource = resources.first();
                return qnResIconCache->icon(resource);
            }
            else if (requiresServerResource(m_eventType))
            {
                return qnResIconCache->icon(QnResourceIconCache::Server);
            }
            else /* ::requiresCameraResource(m_eventType) */
            {
                return qnResIconCache->icon(QnResourceIconCache::Camera);
            }
        }
        case Column::target:
        {
            return iconForAction();
        }
        default:
            break;
    }
    return QIcon();
}

int QnBusinessRuleViewModel::getHelpTopic(Column column) const
{
    switch (column)
    {
        case Column::event:
            return nx::vms::client::desktop::rules::eventHelpId(m_eventType);
        case Column::action:
            return nx::vms::client::desktop::rules::actionHelpId(m_actionType);
        default:
            return HelpTopic::Id::EventsActions;
    }
}

bool QnBusinessRuleViewModel::isValid() const
{
    return m_disabled || (isValid(Column::source) && isValid(Column::target));
}

bool QnBusinessRuleViewModel::isValid(Column column) const
{
    switch (column)
    {
        case Column::source:
        {
            auto filtered = filterEventResources(m_eventResources, m_eventType);
            switch (m_eventType)
            {
                case EventType::cameraMotionEvent:
                    return isResourcesListValid<QnCameraMotionPolicy>(
                        resourcePool()->getResourcesByIds<QnCameraMotionPolicy::resource_type>(filtered));

                case EventType::cameraInputEvent:
                    return isResourcesListValid<QnCameraInputPolicy>(
                        resourcePool()->getResourcesByIds<QnCameraInputPolicy::resource_type>(filtered));

                case EventType::analyticsSdkEvent:
                case EventType::analyticsSdkObjectDetected:
                    return isResourcesListValid<QnCameraAnalyticsPolicy>(
                        resourcePool()->getResourcesByIds<QnCameraAnalyticsPolicy::resource_type>(filtered));

                case EventType::softwareTriggerEvent:
                {
                    if (m_eventParams.metadata.allUsers)
                        return true;

                    if (m_eventParams.metadata.instigators.empty())
                        return false;

                    // TODO: #vkutin #3.2 Create and use proper validation policy,
                    // and avoid code duplication with QnSoftwareTriggerBusinessEventWidget

                    // TODO: #vkutin #3.1 Check camera access permissions

                    QnUserResourceList users;
                    UserGroupDataList groups;
                    nx::vms::common::getUsersAndGroups(systemContext(),
                        m_eventParams.metadata.instigators, users, groups);

                    const auto eventResources = filtered.empty()
                        ? resourcePool()->getAllCameras()
                        : resourcePool()->getResourcesByIds<QnVirtualCameraResource>(filtered);

                    const auto isUserValid =
                        [this, &eventResources](const QnUserResourcePtr& user)
                        {
                            return user->isEnabled()
                                && std::any_of(eventResources.begin(), eventResources.end(),
                                    [this, &user](auto resource)
                                    {
                                        return resourceAccessManager()->hasPermission(
                                            user,
                                            resource,
                                            Qn::Permission::SoftTriggerPermission);
                                    });
                        };

                    const auto isRoleValid =
                        [this, eventResources](const UserGroupData& group)
                        {
                            return std::any_of(eventResources.begin(), eventResources.end(),
                                [this, &group](const auto& resource)
                                {
                                    return resourceAccessManager()->hasPermission(
                                        group,
                                        resource,
                                        Qn::Permission::SoftTriggerPermission);
                                });
                        };

                    return std::any_of(users.cbegin(), users.cend(), isUserValid)
                        || std::any_of(groups.cbegin(), groups.cend(), isRoleValid);
                }

                default:
                    return true;
            }
        }
        case Column::target:
        {
            auto filtered = filterActionResources(this, m_actionResources, m_actionType);

            if (isActionUsingSourceCamera())
                return true;

            switch (m_actionType)
            {
                case ActionType::sendMailAction:
                    return QnSendEmailActionDelegate::isValidList(filtered, m_actionParams.emailAddress);
                case ActionType::cameraRecordingAction:
                    return isResourcesListValid<QnCameraRecordingPolicy>(
                        resourcePool()->getResourcesByIds<QnCameraRecordingPolicy::resource_type>(filtered));
                case ActionType::bookmarkAction:
                    return isResourcesListValid<QnCameraRecordingPolicy>(
                        resourcePool()->getResourcesByIds<QnBookmarkActionPolicy::resource_type>(filtered));
                case ActionType::cameraOutputAction:
                    return isResourcesListValid<QnCameraOutputPolicy>(
                        resourcePool()->getResourcesByIds<QnCameraOutputPolicy::resource_type>(filtered));
                case ActionType::playSoundAction:
                case ActionType::playSoundOnceAction:
                    return !m_actionParams.url.isEmpty()
                        && (isResourcesListValid<QnCameraAudioTransmitPolicy>(
                            resourcePool()->getResourcesByIds<QnCameraAudioTransmitPolicy::resource_type>(filtered))
                            || m_actionParams.playToClient
                        );

                case ActionType::showPopupAction:
                {
                    static const QnDefaultSubjectValidationPolicy defaultPolicy;
                    QnRequiredAccessRightPolicy acknowledgePolicy(
                        nx::vms::api::AccessRight::manageBookmarks);
                    acknowledgePolicy.setCameras(
                        resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                            filterEventResources(m_eventResources, m_eventType)));

                    const auto subjects = filterSubjectIds(m_actionParams.additionalResources);
                    const auto validationState = m_actionParams.needConfirmation
                        ? acknowledgePolicy.validity(m_actionParams.allUsers, subjects)
                        : defaultPolicy.validity(m_actionParams.allUsers, subjects);

                    return validationState != QValidator::Invalid;
                }
                case ActionType::pushNotificationAction:
                {
                    if (systemSettings()->cloudSystemId().isEmpty())
                        return false;

                    static const QnCloudUsersValidationPolicy cloudUsersPolicy;
                    const auto subjects = filterSubjectIds(m_actionParams.additionalResources);
                    const auto validationState = cloudUsersPolicy.validity(
                        m_actionParams.allUsers,
                        subjects);

                    return validationState != QValidator::Invalid;
                }

                case ActionType::sayTextAction:
                    return !m_actionParams.sayText.isEmpty()
                        && (isResourcesListValid<QnCameraAudioTransmitPolicy>(
                            resourcePool()->getResourcesByIds<QnCameraAudioTransmitPolicy::resource_type>(filtered))
                            || m_actionParams.playToClient
                        );

                case ActionType::executePtzPresetAction:
                    return isResourcesListValid<QnExecPtzPresetPolicy>(
                        resourcePool()->getResourcesByIds<QnExecPtzPresetPolicy::resource_type>(filtered))
                        && m_actionResources.size() == 1
                        && !m_actionParams.presetId.isEmpty();

                case ActionType::execHttpRequestAction:
                {
                    QUrl url(m_actionParams.url);
                    return url.isValid()
                        && !url.isEmpty()
                        && (url.scheme().isEmpty() || nx::network::http::isUrlScheme(url.scheme()))
                        && !url.host().isEmpty();
                }
                case ActionType::openLayoutAction:
                {
                    const auto layout = resourcePool()->getResourceById<QnLayoutResource>(
                        m_actionParams.actionResourceId);

                    if (!layout)
                        return false;

                    if (m_actionParams.allUsers)
                        return true;

                    QnLayoutAccessValidationPolicy layoutAccessPolicy{};
                    layoutAccessPolicy.setLayout(layout);

                    // TODO: use iterator-based constructor after update to Qt 5.14.
                    QSet<QnUuid> subjects;
                    for (const auto& v: m_actionParams.additionalResources)
                        subjects << v;

                    return layoutAccessPolicy.validity(m_actionParams.allUsers, subjects) != QValidator::Invalid;
                }
                case ActionType::buzzerAction:
                {
                    return !filtered.isEmpty() || actionIsUsingSourceServer();
                }
                default:
                    break;
            }

            // TODO: #sivanov Check all variants or resource requirements:
            // userResource, serverResource, etc.
            auto resources = resourcePool()->getResourcesByIds(filtered);
            if (requiresCameraResource(m_actionType) && resources.isEmpty())
            {
                return false;
            }
        }
        default:
            break;
    }
    return true;
}

void QnBusinessRuleViewModel::updateEventStateModel()
{
    m_eventStatesModel->clear();
    for (EventState val:
        allowedEventStates(m_eventType, m_eventParams, systemContext()))
    {
        QStandardItem *item = new QStandardItem(toggleStateToModelString(val));
        item->setData(int(val));

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
};

void QnBusinessRuleViewModel::updateActionTypesModel()
{
    QModelIndexList prolongedActions = m_actionTypesModel->match(m_actionTypesModel->index(0, 0),
        ProlongedActionRole, true, -1, Qt::MatchExactly);

    // what type of actions to show: prolonged or instant
    bool enableProlongedActions =
        hasToggleState(m_eventType, m_eventParams, systemContext());
    foreach(QModelIndex idx, prolongedActions)
    {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }
}

QString QnBusinessRuleViewModel::getSourceText(bool detailed) const
{
    QnResourceList resources = resourcePool()->getResourcesByIds(eventResources());
    if (m_eventType == EventType::cameraMotionEvent)
        return QnCameraMotionPolicy::getText(resources, detailed);

    if (m_eventType == EventType::cameraInputEvent)
        return QnCameraInputPolicy::getText(resources, detailed);

    if (m_eventType == EventType::analyticsSdkEvent)
        return QnCameraAnalyticsPolicy::getText(resources, detailed);

    if (m_eventType == EventType::analyticsSdkObjectDetected)
        return QnCameraAnalyticsPolicy::getText(resources, detailed);

    if (!isResourceRequired(m_eventType))
        return braced(tr("System"));

    if (resources.size() == 1)
    {
        return QnResourceDisplayInfo(resources.first()).toString(
            appContext()->localSettings()->resourceInfoLevel());
    }

    if (requiresServerResource(m_eventType))
    {
        if (resources.isEmpty())
            return braced(tr("Any Server"));

        return tr("%n Servers", "", resources.size());
    }

    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
    {
        return braced(QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Any Device"),
            tr("Any Camera")
        ));
    }

    return QnDeviceDependentStrings::getNumericName(resourcePool(), cameras);
}

QString QnBusinessRuleViewModel::getTargetText(bool detailed) const
{
    if (isActionUsingSourceCamera())
    {
        QnVirtualCameraResourceList targetCameras =
            resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_actionResources);

        if (targetCameras.isEmpty())
            return tr("Source Camera");

        return tr("Source and %n more Cameras", "", targetCameras.size());
    }

    QnResourceList resources = resourcePool()->getResourcesByIds(actionResources());

    switch (m_actionType)
    {
        case ActionType::sendMailAction:
        {
            return QnSendEmailActionDelegate::getText(actionResources(),
                detailed,
                m_actionParams.emailAddress);
        }
        case ActionType::openLayoutAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        {
            if (m_actionParams.allUsers)
                return StringsHelper::allUsersText();
            QnUserResourceList users;
            UserGroupDataList groups;
            nx::vms::common::getUsersAndGroups(systemContext(),
                m_actionParams.additionalResources, users, groups);
            users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
            return m_helper->actionSubjects(users, groups);
        }
        case ActionType::bookmarkAction:
        case ActionType::cameraRecordingAction:
        {
            return QnCameraRecordingPolicy::getText(resources, detailed);
        }
        case ActionType::cameraOutputAction:
        {
            return QnCameraOutputPolicy::getText(resources, detailed);
        }

        case ActionType::playSoundAction:
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
            return QnCameraAudioTransmitPolicy::getText(resources, detailed);

        case ActionType::executePtzPresetAction:
            return QnExecPtzPresetPolicy::getText(resources, detailed);

        case ActionType::fullscreenCameraAction:
        {
            return detailed
                ? FullscreenActionHelper::tableCellText(this)
                : FullscreenActionHelper::cameraText(this);
        }

        case ActionType::exitFullscreenAction:
        {
            return ExitFullscreenActionHelper::tableCellText(this);
        }

        case ActionType::execHttpRequestAction:
            return QUrl(m_actionParams.url).toString(QUrl::RemoveUserInfo);

        default:
            break;
    }

    if (requiresCameraResource(m_actionType))
    {
        QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
        if (cameras.size() == 1)
        {
            return QnResourceDisplayInfo(cameras.first()).toString(
                appContext()->localSettings()->resourceInfoLevel());
        }

        if (cameras.isEmpty())
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Select at least one Device"),
                tr("Select at least one Camera")
            );

        return QnDeviceDependentStrings::getNumericName(resourcePool(), cameras);
    }

    if (requiresServerResource(m_actionType))
    {
        const QnMediaServerResourceList targetServers =
            resourcePool()->getResourcesByIds<QnMediaServerResource>(m_actionResources);

        if (targetServers.isEmpty())
            return actionIsUsingSourceServer() ? tr("Source Server") : tr("Select Server");

        const auto targetServersString = (targetServers.count() > 1)
            ? tr("%n Servers", "", targetServers.count())
            : targetServers.first()->getName();

        return actionIsUsingSourceServer()
            ? tr("Source Server and %1").arg(targetServersString)
            : targetServersString;
    }

    if (requiresUserResource(m_actionType))
    {
        return braced(tr("User"));
    }

    return braced(tr("System"));
}

QString QnBusinessRuleViewModel::getAggregationText() const
{
    if (!allowsAggregation(m_actionType))
        return tr("N/A");

    if (m_aggregationPeriodSec <= 0)
        return tr("Instant");

    const auto duration = std::chrono::seconds(m_aggregationPeriodSec);
    static const QString kSeparator(' ');

    using HumanReadable = nx::vms::text::HumanReadable;
    const auto timespan = HumanReadable::timeSpan(duration,
        HumanReadable::DaysAndTime,
        HumanReadable::SuffixFormat::Full,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);

    return tr("Every %1").arg(timespan);
}

QString QnBusinessRuleViewModel::toggleStateToModelString(EventState value)
{
    switch (value)
    {
        case EventState::inactive:  return tr("Stops");
        case EventState::active:    return tr("Starts");
        case EventState::undefined: return tr("Occurs");

        default:
            NX_ASSERT(false, "Unknown EventState enumeration value.");
            return QString();
    }
}
