#include "business_rule_view_model.h"

#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/resource_display_info.h>

// TODO: #vkutin Move these to proper locations and namespaces
#include <business/business_resource_validation.h>
#include <business/business_types_comparator.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <client/client_settings.h>

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/events/events.h>
#include <nx/vms/event/actions/actions.h>
#include <nx/vms/client/desktop/event_rules/helpers/fullscreen_action_helper.h>
#include <nx/vms/client/desktop/event_rules/helpers/exit_fullscreen_action_helper.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/business_help.h>
#include <ui/models/notification_sound_model.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/core/utils/human_readable.h>
#include <utils/media/audio_player.h>
#include <nx/network/http/http_types.h>

#include <nx/fusion/model_functions.h>

using namespace nx;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;
using namespace nx::vms::client::desktop;

namespace {

const int ProlongedActionRole = Qt::UserRole + 2;
const int defaultActionDurationMs = 5000;
const int defaultAggregationPeriodSec = 60;

QString braced(const QString& source)
{
    return L'<' + source + L'>';
};

QVector<QnUuid> toIdList(const QSet<QnUuid>& src)
{
    QVector<QnUuid> result;
    result.reserve(src.size());
    for (auto id : src)
        result << id;
    return result;
}

QSet<QnUuid> toIdSet(const QVector<QnUuid>& src)
{
    QSet<QnUuid> result;
    result.reserve(src.size());
    for (auto id : src)
        result << id;
    return result;
}

QSet<QnUuid> toIds(const QnResourceList& resources)
{
    QSet<QnUuid> result;
    for (auto resource : resources)
        result << resource->getId();

    return result;
}

QSet<QnUuid> filterEventResources(const QSet<QnUuid>& ids, vms::api::EventType eventType)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    if (vms::event::requiresCameraResource(eventType))
        return toIds(resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids));

    if (vms::event::requiresServerResource(eventType))
        return toIds(resourcePool->getResourcesByIds<QnMediaServerResource>(ids));

    return QSet<QnUuid>();
}

template<class IDList>
QSet<QnUuid> filterSubjectIds(const IDList& ids)
{
    QnUserResourceList users;
    QList<QnUuid> roles;
    qnClientCoreModule->commonModule()->userRolesManager()->usersAndRoles(ids, users, roles);
    return toIds(users).unite(roles.toSet());
}

QSet<QnUuid> filterActionResources(
    const QnBusinessRuleViewModel* model,
    const QSet<QnUuid>& ids,
    vms::api::ActionType actionType)
{
    if (actionType == vms::api::ActionType::fullscreenCameraAction)
    {
        return FullscreenActionHelper::layoutIds(model) | FullscreenActionHelper::cameraIds(model);
    }
    if (actionType == vms::api::ActionType::exitFullscreenAction)
    {
        return ExitFullscreenActionHelper::layoutIds(model);
    }


    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    if (vms::event::requiresCameraResource(actionType))
        return toIds(resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids));

    if (vms::event::requiresUserResource(actionType))
        return filterSubjectIds(ids);

    return QSet<QnUuid>();
}

/**
*  This method must cleanup all action parameters that are not required for the given action.
*  // TODO: #GDM implement correct filtering
*/
vms::event::ActionParameters filterActionParams(vms::api::ActionType actionType, const vms::event::ActionParameters &params)
{
    Q_UNUSED(actionType);
    return params;
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
    m_eventState(vms::api::EventState::undefined),
    m_actionType(ActionType::showPopupAction),
    m_aggregationPeriodSec(defaultAggregationPeriodSec),
    m_disabled(false),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this)),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    const auto addEventItem =
        [this](vms::api::EventType eventType)
        {
            auto item = new QStandardItem(m_helper->eventName(eventType));
            item->setData(eventType);
            m_eventTypesModel->appendRow(item);
        };

    const auto addActionItem =
        [this](vms::event::ActionType actionType)
        {
            auto item = new QStandardItem(m_helper->actionName(actionType));
            item->setData(actionType);
            item->setData(!vms::event::canBeInstant(actionType), ProlongedActionRole);
            m_actionTypesModel->appendRow(item);
        };

    const auto addSeparator =
        [](QStandardItemModel* model)
        {
            auto item = new QStandardItem(lit("-"));
            item->setData(lit("separator"), Qt::AccessibleDescriptionRole);
            model->appendRow(item);
        };

    using EventSubType = QnBusinessTypesComparator::EventSubType;

    QnBusinessTypesComparator lexComparator(true);
    for (const auto eventType: lexComparator.lexSortedEvents(EventSubType::user))
    {
        if (eventType == EventType::pluginEvent)
        {
            /** Show it only for users who have at least one plugin installed. */
            const auto pirs =
                resourcePool()->getResources<nx::vms::common::AnalyticsEngineResource>();
            if (pirs.isEmpty())
                continue;
        }
        addEventItem(eventType);
    }
    addSeparator(m_eventTypesModel);
    for (const auto eventType: lexComparator.lexSortedEvents(EventSubType::failure))
        addEventItem(eventType);
    addSeparator(m_eventTypesModel);
    for (const auto eventType: lexComparator.lexSortedEvents(EventSubType::success))
        addEventItem(eventType);

    using ActionSubType = QnBusinessTypesComparator::ActionSubType;
    for (const auto actionType: lexComparator.lexSortedActions(ActionSubType::server))
        addActionItem(actionType);
    addSeparator(m_actionTypesModel);
    for (const auto actionType: lexComparator.lexSortedActions(ActionSubType::client))
        addActionItem(actionType);

    m_actionParams.additionalResources = userRolesManager()->adminRoleIds().toVector().toStdVector();

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

        case Qn::UuidRole:
            return qVariantFromValue(m_id);
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
            return qVariantFromValue(m_eventType);
        case Qn::EventParametersRole:
            return qVariantFromValue(m_eventParams);
        case Qn::EventResourcesRole:
            return qVariantFromValue(filterEventResources(m_eventResources, m_eventType));
        case Qn::ActionTypeRole:
            return qVariantFromValue(m_actionType);
        case Qn::ActionResourcesRole:
        {
            auto ids = m_actionType != ActionType::showPopupAction
                ? filterActionResources(this, m_actionResources, m_actionType)
                : filterSubjectIds(m_actionParams.additionalResources);

            if (m_actionParams.allUsers)
                ids.insert(kAllUsersId);

            return qVariantFromValue(ids);
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
            setEventType((vms::api::EventType)value.toInt());
            return true;

        case Column::action:
            setActionType((vms::api::ActionType)value.toInt());
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
                case ActionType::showPopupAction:
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


void QnBusinessRuleViewModel::loadFromRule(const vms::event::RulePtr& businessRule)
{
    m_id = businessRule->id();
    m_modified = false;
    if (m_eventType != businessRule->eventType())
    {
        m_eventType = businessRule->eventType();
        updateEventStateModel();
    }

    m_eventResources = filterEventResources(toIdSet(businessRule->eventResources()),
        m_eventType);

    m_eventParams = businessRule->eventParams();

    m_eventState = businessRule->eventState();

    m_actionType = businessRule->actionType();

    m_actionResources = toIdSet(businessRule->actionResources());

    m_actionParams = businessRule->actionParams();

    m_aggregationPeriodSec = businessRule->aggregationPeriod();

    m_disabled = businessRule->isDisabled();
    m_comments = businessRule->comment();
    m_schedule = businessRule->schedule();

    updateActionTypesModel();// TODO: #GDM #Business connect on dataChanged?

    emit dataChanged(Field::all);
}

vms::event::RulePtr QnBusinessRuleViewModel::createRule() const
{
    vms::event::RulePtr rule(new vms::event::Rule());
    rule->setId(m_id);
    rule->setEventType(m_eventType);
    rule->setEventResources(toIdList(filterEventResources(m_eventResources, m_eventType)));
    rule->setEventState(m_eventState);   // TODO: #GDM #Business check
    rule->setEventParams(m_eventParams); // TODO: #GDM #Business filtered
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

vms::api::EventType QnBusinessRuleViewModel::eventType() const
{
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const vms::api::EventType value)
{
    if (m_eventType == value)
        return;

    bool cameraRequired = vms::event::requiresCameraResource(m_eventType);
    bool serverRequired = vms::event::requiresServerResource(m_eventType);

    // Store params for the current event type
    m_cachedEventParams[m_eventType] = m_eventParams;

    m_eventType = value;
    m_modified = true;

    // Create or load params for the new event type
    if (!m_cachedEventParams.contains(m_eventType))
    {
        nx::vms::event::EventParameters eventParams;

        switch (m_eventType)
        {
            case EventType::pluginEvent:
            {
                using namespace nx::vms::api;
                EventLevels levels = 0;
                levels |= ErrorEventLevel;
                levels |= WarningEventLevel;
                levels |= InfoEventLevel;
                eventParams.inputPortId = QnLexical::serialized(levels);
                break;
            }

            case EventType::softwareTriggerEvent:
                eventParams.inputPortId = QnUuid::createUuid().toSimpleString();
                eventParams.description = QnSoftwareTriggerPixmaps::defaultPixmapName();
                break;

            default:
                break;
        }

        m_cachedEventParams[m_eventType] = eventParams;
    }
    m_eventParams = m_cachedEventParams[m_eventType];

    Fields fields = Field::eventType | Field::modified;

    if (vms::event::requiresCameraResource(m_eventType) != cameraRequired ||
        vms::event::requiresServerResource(m_eventType) != serverRequired)
    {
        fields |= Field::eventResources;
    }

    fields |= updateEventClassRelatedParams();

    emit dataChanged(fields);
    // TODO: #GDM #Business check others, params and resources should be merged
}

QnBusinessRuleViewModel::Fields QnBusinessRuleViewModel::updateEventClassRelatedParams()
{
    Fields fields = Field::modified;
    if (vms::event::hasToggleState(m_eventType, m_eventParams, commonModule()))
    {
        if (!isActionProlonged())
        {
            const auto allowedStates = allowedEventStates(m_eventType, m_eventParams, commonModule());
            if (!allowedStates.contains(m_eventState))
            {
                m_eventState = allowedStates.first();
                fields |= Field::eventState;
            }
        }
    }
    else
    {
        if (m_eventState != vms::api::EventState::undefined)
        {
            m_eventState = vms::api::EventState::undefined;
            fields |= Field::eventState;
        }

        if (!vms::event::canBeInstant(m_actionType))
        {
            m_actionType = ActionType::showPopupAction;
            fields |= Field::actionType | Field::actionResources | Field::actionParams;
        }
        else if (isActionProlonged() && vms::event::supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
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
    switch (m_actionType)
    {
        case vms::event::ActionType::sendMailAction:
        {
            if (!isValid(Column::target))
                return qnSkin->icon("tree/user_alert.png");
            return qnResIconCache->icon(QnResourceIconCache::Users);
        }

        case vms::event::ActionType::showPopupAction:
        {
            if (m_actionParams.allUsers)
                return qnResIconCache->icon(QnResourceIconCache::Users);
            if (!isValid(Column::target))
                return qnSkin->icon("tree/user_alert.png");

            QnUserResourceList users;
            QList<QnUuid> roles;
            userRolesManager()->usersAndRoles(m_actionParams.additionalResources, users, roles);
            users = users.filtered(
                [](const QnUserResourcePtr& user) { return user->isEnabled(); });
            return (users.size() > 1 || !roles.empty())
                ? qnResIconCache->icon(QnResourceIconCache::Users)
                : qnResIconCache->icon(QnResourceIconCache::User);
        }

        case vms::event::ActionType::showTextOverlayAction:
        case vms::event::ActionType::showOnAlarmLayoutAction:
        {
            if (isUsingSourceCamera())
                return qnResIconCache->icon(QnResourceIconCache::Camera);
            break;
        }

        case vms::event::ActionType::fullscreenCameraAction:
        {
            return FullscreenActionHelper::tableCellIcon(this);
        }

        case vms::event::ActionType::exitFullscreenAction:
        {
            return ExitFullscreenActionHelper::tableCellIcon(this);
        }

        default:
            break;
    }

    // TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
    QnResourceList resources = resourcePool()->getResourcesByIds(actionResources());
    if (!vms::event::requiresCameraResource(m_actionType))
        return qnResIconCache->icon(QnResourceIconCache::Servers);

    if (resources.size() == 1)
        return qnResIconCache->icon(resources.first());

    if (resources.isEmpty())
        return qnSkin->icon(lit("tree/buggy.png"));

    return qnResIconCache->icon(QnResourceIconCache::Camera);
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

vms::event::EventParameters QnBusinessRuleViewModel::eventParams() const
{
    return m_eventParams;
}

void QnBusinessRuleViewModel::setEventParams(const vms::event::EventParameters &params)
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

vms::api::EventState QnBusinessRuleViewModel::eventState() const
{
    return m_eventState;
}

void QnBusinessRuleViewModel::setEventState(vms::api::EventState state)
{
    if (m_eventState == state)
        return;

    m_eventState = state;
    m_modified = true;

    emit dataChanged(Field::eventState | Field::modified);
}

vms::api::ActionType QnBusinessRuleViewModel::actionType() const
{
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const vms::api::ActionType value)
{
    if (m_actionType == value)
        return;

    const bool cameraWasRequired = vms::event::requiresCameraResource(m_actionType);
    const bool userWasRequired = vms::event::requiresUserResource(m_actionType);
    const bool additionalUserWasRequired = vms::event::requiresAdditionalUserResource(m_actionType);

    const bool wasEmailAction = m_actionType == ActionType::sendMailAction;
    const bool aggregationWasDisabled = !vms::event::allowsAggregation(m_actionType);

    m_actionType = value;
    m_modified = true;

    const bool cameraIsRequired = vms::event::requiresCameraResource(m_actionType);
    const bool userIsRequired = vms::event::requiresUserResource(m_actionType);
    const bool additionalUserIsRequired = vms::event::requiresAdditionalUserResource(m_actionType);

    if (userIsRequired && !userWasRequired)
        m_actionResources = userRolesManager()->adminRoleIds().toSet();

    Fields fields = Field::actionType | Field::modified;
    if (cameraIsRequired != cameraWasRequired || userIsRequired != userWasRequired)
        fields |= Field::actionResources;

    if (additionalUserWasRequired != additionalUserIsRequired)
    {
        fields |= Field::actionParams;
        m_actionParams.allUsers = false;
        m_actionParams.additionalResources = additionalUserIsRequired
            ? userRolesManager()->adminRoleIds().toVector().toStdVector()
            : std::vector<QnUuid>();
    }

    if (!vms::event::allowsAggregation(m_actionType))
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

    if (vms::event::hasToggleState(m_eventType, m_eventParams, commonModule()) &&
        !isActionProlonged() && m_eventState == vms::api::EventState::undefined)
    {
        m_eventState = allowedEventStates(m_eventType, m_eventParams, commonModule()).first();
        fields |= Field::eventState;
    }
    else if (!vms::event::hasToggleState(m_eventType, m_eventParams, commonModule()) &&
        vms::event::supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
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
    return vms::event::isActionProlonged(m_actionType, m_actionParams);
}

vms::event::ActionParameters QnBusinessRuleViewModel::actionParams() const
{
    return m_actionParams;
}

void QnBusinessRuleViewModel::setActionParams(const vms::event::ActionParameters &params)
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

bool QnBusinessRuleViewModel::canUseSourceCamera() const
{
    return m_eventType >= vms::api::userDefinedEvent
        || vms::event::requiresCameraResource(m_eventType);
}

bool QnBusinessRuleViewModel::isUsingSourceCamera() const
{
    return m_actionParams.useSource && canUseSourceCamera();
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
            // TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            auto resources = resourcePool()->getResourcesByIds(eventResources());
            if (!vms::event::isResourceRequired(m_eventType))
            {
                return qnResIconCache->icon(QnResourceIconCache::CurrentSystem);
            }
            else if (resources.size() == 1)
            {
                QnResourcePtr resource = resources.first();
                return qnResIconCache->icon(resource);
            }
            else if (vms::event::requiresServerResource(m_eventType))
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
        case Column::event:  return QnBusiness::eventHelpId(m_eventType);
        case Column::action: return QnBusiness::actionHelpId(m_actionType);
        default:             return Qn::EventsActions_Help;
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
                    QList<QnUuid> roles;
                    userRolesManager()->usersAndRoles(m_eventParams.metadata.instigators, users, roles);

                    const auto isUserValid =
                        [this](const QnUserResourcePtr& user)
                        {
                            return user->isEnabled() && resourceAccessManager()->hasGlobalPermission(
                                user, GlobalPermission::userInput);
                        };

                    const auto isRoleValid =
                        [this](const QnUuid& roleId)
                        {
                            const auto role = userRolesManager()->predefinedRole(roleId);
                            switch (role)
                            {
                                case Qn::UserRole::customPermissions:
                                    return false;
                                case Qn::UserRole::customUserRole:
                                {
                                    const auto customRole = userRolesManager()->userRole(roleId);
                                    return customRole.permissions.testFlag(GlobalPermission::userInput);
                                }
                                default:
                                {
                                    const auto permissions = userRolesManager()->userRolePermissions(role);
                                    return permissions.testFlag(GlobalPermission::userInput);
                                }
                            }
                        };

                    return std::any_of(users.cbegin(), users.cend(), isUserValid)
                        || std::any_of(roles.cbegin(), roles.cend(), isRoleValid);
                }

                default:
                    return true;
            }
        }
        case Column::target:
        {
            auto filtered = filterActionResources(this, m_actionResources, m_actionType);
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
                    static const QnRequiredPermissionSubjectPolicy acknowledgePolicy(
                        GlobalPermission::manageBookmarks, QString());

                    const auto subjects = filterSubjectIds(m_actionParams.additionalResources);
                    const auto validationState = m_actionParams.needConfirmation
                        ? acknowledgePolicy.validity(m_actionParams.allUsers, subjects)
                        : defaultPolicy.validity(m_actionParams.allUsers, subjects);

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
                case vms::api::showTextOverlayAction:
                case vms::api::showOnAlarmLayoutAction:
                {
                    if (isUsingSourceCamera())
                        return true;
                    break;
                }
                case ActionType::execHttpRequestAction:
                {
                    QUrl url(m_actionParams.url);
                    return url.isValid()
                        && !url.isEmpty()
                        && (url.scheme().isEmpty() || nx::network::http::isUrlSheme(url.scheme()))
                        && !url.host().isEmpty();
                }
                default:
                    break;
            }

            // TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            auto resources = resourcePool()->getResourcesByIds(filtered);
            if (vms::event::requiresCameraResource(m_actionType) && resources.isEmpty())
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
    foreach(vms::api::EventState val, vms::event::allowedEventStates(m_eventType, m_eventParams, commonModule()))
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
    bool enableProlongedActions = vms::event::hasToggleState(m_eventType, m_eventParams, commonModule());
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

    if (!vms::event::isResourceRequired(m_eventType))
        return braced(tr("System"));

    if (resources.size() == 1)
        return QnResourceDisplayInfo(resources.first()).toString(qnSettings->extraInfoInTree());

    if (vms::event::requiresServerResource(m_eventType))
    {
        if (resources.isEmpty())
            return braced(tr("Any Server"));

        return tr("%n Server(s)", "", resources.size());
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
    QnResourceList resources = resourcePool()->getResourcesByIds(actionResources());
    switch (m_actionType)
    {
        case ActionType::sendMailAction:
        {
            return QnSendEmailActionDelegate::getText(actionResources(),
                detailed,
                m_actionParams.emailAddress);
        }
        case ActionType::showPopupAction:
        {
            if (m_actionParams.allUsers)
                return nx::vms::event::StringsHelper::allUsersText();
            QnUserResourceList users;
            QList<QnUuid> roles;
            userRolesManager()->usersAndRoles(m_actionParams.additionalResources, users, roles);
            users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
            return m_helper->actionSubjects(users, roles);
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

        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        {
            if (isUsingSourceCamera())
            {
                QnVirtualCameraResourceList targetCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_actionResources);

                if (targetCameras.isEmpty())
                    return tr("Source camera");

                return tr("Source and %n more cameras", "", targetCameras.size());
            }
            break;
        }

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

    // TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
    if (!vms::event::requiresCameraResource(m_actionType))
        return braced(tr("System"));

    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.size() == 1)
        return QnResourceDisplayInfo(cameras.first()).toString(qnSettings->extraInfoInTree());

    if (cameras.isEmpty())
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Select at least one device"),
            tr("Select at least one camera")
        );

    return QnDeviceDependentStrings::getNumericName(resourcePool(), cameras);
}

QString QnBusinessRuleViewModel::getAggregationText() const
{
    if (!vms::event::allowsAggregation(m_actionType))
        return tr("N/A");

    if (m_aggregationPeriodSec <= 0)
        return tr("Instant");

    const auto duration = std::chrono::seconds(m_aggregationPeriodSec);
    static const QString kSeparator(L' ');

    using HumanReadable = nx::vms::client::core::HumanReadable;
    const auto timespan = HumanReadable::timeSpan(duration,
        HumanReadable::DaysAndTime,
        HumanReadable::SuffixFormat::Full,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);

    return tr("Every %1").arg(timespan);
}

QString QnBusinessRuleViewModel::toggleStateToModelString(vms::api::EventState value)
{
    switch (value)
    {
        case vms::api::EventState::inactive:  return tr("Stops");
        case vms::api::EventState::active:    return tr("Starts");
        case vms::api::EventState::undefined: return tr("Occurs");

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Unknown EventState enumeration value.");
            return QString();
    }
}
