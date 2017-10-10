#include "business_rule_view_model.h"

#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
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

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/events/events.h>
#include <nx/vms/event/actions/actions.h>

#include <ui/help/help_topics.h>
#include <ui/help/business_help.h>
#include <ui/models/notification_sound_model.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/common/qtimespan.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>

using namespace nx;

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

QSet<QnUuid> filterEventResources(const QSet<QnUuid>& ids, vms::event::EventType eventType)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    if (vms::event::requiresCameraResource(eventType))
        return toIds(resourcePool->getResources<QnVirtualCameraResource>(ids));

    if (vms::event::requiresServerResource(eventType))
        return toIds(resourcePool->getResources<QnMediaServerResource>(ids));

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

QSet<QnUuid> filterActionResources(const QSet<QnUuid>& ids, vms::event::ActionType actionType)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    if (vms::event::requiresCameraResource(actionType))
        return toIds(resourcePool->getResources<QnVirtualCameraResource>(ids));

    if (requiresUserResource(actionType))
        return filterSubjectIds(ids);

    return QSet<QnUuid>();
}

/**
*  This method must cleanup all action parameters that are not required for the given action.
*  // TODO: #GDM implement correct filtering
*/
vms::event::ActionParameters filterActionParams(vms::event::ActionType actionType, const vms::event::ActionParameters &params)
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
    m_eventType(vms::event::cameraDisconnectEvent),
    m_eventState(vms::event::EventState::undefined),
    m_actionType(vms::event::showPopupAction),
    m_aggregationPeriodSec(defaultAggregationPeriodSec),
    m_disabled(false),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this)),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    QnBusinessTypesComparator lexComparator(true);
    for (vms::event::EventType eventType : lexComparator.lexSortedEvents())
    {
        QStandardItem *item = new QStandardItem(m_helper->eventName(eventType));
        item->setData(eventType);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }

    for (vms::event::ActionType actionType : lexComparator.lexSortedActions())
    {
        QStandardItem *item = new QStandardItem(m_helper->actionName(actionType));
        item->setData(actionType);
        item->setData(!vms::event::canBeInstant(actionType), ProlongedActionRole);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }

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
                    if (m_actionType == vms::event::sendMailAction)
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
        case Qn::EventResourcesRole:
            return qVariantFromValue(filterEventResources(m_eventResources, m_eventType));
        case Qn::ActionTypeRole:
            return qVariantFromValue(m_actionType);
        case Qn::ActionResourcesRole:
        {
            auto ids = m_actionType != vms::event::showPopupAction
                ? filterActionResources(m_actionResources, m_actionType)
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
            setEventType((vms::event::EventType)value.toInt());
            return true;

        case Column::action:
            setActionType((vms::event::ActionType)value.toInt());
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
                case vms::event::showPopupAction:
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


void QnBusinessRuleViewModel::loadFromRule(vms::event::RulePtr businessRule)
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

    m_actionResources = filterActionResources(toIdSet(businessRule->actionResources()),
        m_actionType);

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
    rule->setActionResources(toIdList(filterActionResources(m_actionResources, m_actionType)));
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

vms::event::EventType QnBusinessRuleViewModel::eventType() const
{
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const vms::event::EventType value)
{
    if (m_eventType == value)
        return;

    bool cameraRequired = vms::event::requiresCameraResource(m_eventType);
    bool serverRequired = vms::event::requiresServerResource(m_eventType);

    m_eventType = value;
    m_modified = true;

    Fields fields = Field::eventType | Field::modified;

    if (vms::event::requiresCameraResource(m_eventType) != cameraRequired ||
        vms::event::requiresServerResource(m_eventType) != serverRequired)
    {
        fields |= Field::eventResources;
    }

    if (vms::event::hasToggleState(m_eventType))
    {
        if (!isActionProlonged())
        {
            const auto allowedStates = allowedEventStates(m_eventType);
            if (!allowedStates.contains(m_eventState))
            {
                m_eventState = allowedStates.first();
                fields |= Field::eventState;
            }
        }
    }
    else
    {
        if (m_eventState != vms::event::EventState::undefined)
        {
            m_eventState = vms::event::EventState::undefined;
            fields |= Field::eventState;
        }

        if (!vms::event::canBeInstant(m_actionType))
        {
            m_actionType = vms::event::showPopupAction;
            fields |= Field::actionType | Field::actionResources | Field::actionParams;
        }
        else if (isActionProlonged() && vms::event::supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
        {
            m_actionParams.durationMs = defaultActionDurationMs;
            fields |= Field::actionParams;
        }
    }

    switch (m_eventType)
    {
        case vms::event::cameraInputEvent:
            m_eventParams.inputPortId = QString();
            break;

        case vms::event::softwareTriggerEvent:
            m_eventParams.inputPortId = QnUuid::createUuid().toSimpleString();
            m_eventParams.description = QnSoftwareTriggerPixmaps::defaultPixmapName();
            m_eventParams.caption = QString();
            break;

        default:
            m_eventParams.caption = m_eventParams.description = QString();
    }

    updateActionTypesModel();
    updateEventStateModel();

    emit dataChanged(fields);
    // TODO: #GDM #Business check others, params and resources should be merged
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
    bool hasChanges = !(m_eventParams == params);

    if (!hasChanges)
        return;

    m_eventParams = params;
    m_modified = true;

    emit dataChanged(Field::eventParams | Field::modified);
}

vms::event::EventState QnBusinessRuleViewModel::eventState() const
{
    return m_eventState;
}

void QnBusinessRuleViewModel::setEventState(vms::event::EventState state)
{
    if (m_eventState == state)
        return;

    m_eventState = state;
    m_modified = true;

    emit dataChanged(Field::eventState | Field::modified);
}

vms::event::ActionType QnBusinessRuleViewModel::actionType() const
{
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const vms::event::ActionType value)
{
    if (m_actionType == value)
        return;

    const bool cameraWasRequired = vms::event::requiresCameraResource(m_actionType);
    const bool userWasRequired = vms::event::requiresUserResource(m_actionType);
    const bool additionalUserWasRequired = vms::event::requiresAdditionalUserResource(m_actionType);

    const bool wasEmailAction = m_actionType == vms::event::sendMailAction;
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
        if (value == vms::event::sendMailAction && m_aggregationPeriodSec == defaultAggregationPeriodSec)
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

    if (vms::event::hasToggleState(m_eventType) && !isActionProlonged() && m_eventState == vms::event::EventState::undefined)
    {
        m_eventState = allowedEventStates(m_eventType).first();
        fields |= Field::eventState;
    }
    else if (!vms::event::hasToggleState(m_eventType) && vms::event::supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
    {
        m_actionParams.durationMs = defaultActionDurationMs;
        fields |= Field::actionParams;
    }

    emit dataChanged(fields);
}

QSet<QnUuid> QnBusinessRuleViewModel::actionResources() const
{
    return filterActionResources(m_actionResources, m_actionType);
}

void QnBusinessRuleViewModel::setActionResources(const QSet<QnUuid>& value)
{
    auto filtered = filterActionResources(value, m_actionType);
    auto oldFiltered = filterActionResources(m_actionResources, m_actionType);

    if (filtered == oldFiltered)
        return;

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

void QnBusinessRuleViewModel::setDisabled(const bool value)
{
    if (m_disabled == value)
        return;

    m_disabled = value;
    m_modified = true;

    emit dataChanged(Field::all); // all fields should be redrawn
}

QString QnBusinessRuleViewModel::comments() const
{
    return m_comments;
}

void QnBusinessRuleViewModel::setComments(const QString value)
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

void QnBusinessRuleViewModel::setSchedule(const QString value)
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

QString QnBusinessRuleViewModel::getText(Column column, const bool detailed) const
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
            auto resources = resourcePool()->getResources(eventResources());
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
            switch (m_actionType)
            {
                case vms::event::sendMailAction:
                {
                    if (!isValid(Column::target))
                        return qnSkin->icon("tree/user_alert.png");
                    return qnResIconCache->icon(QnResourceIconCache::Users);
                }

                case vms::event::showPopupAction:
                {
                    if (m_actionParams.allUsers)
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                    if (!isValid(Column::target))
                        return qnSkin->icon("tree/user_alert.png");

                    QnUserResourceList users;
                    QList<QnUuid> roles;
                    userRolesManager()->usersAndRoles(m_actionParams.additionalResources, users, roles);
                    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
                    return (users.size() > 1 || !roles.empty())
                        ? qnResIconCache->icon(QnResourceIconCache::Users)
                        : qnResIconCache->icon(QnResourceIconCache::User);
                }

                case vms::event::showTextOverlayAction:
                case vms::event::showOnAlarmLayoutAction:
                {
                    bool canUseSource = (m_actionParams.useSource && (m_eventType >= vms::event::userDefinedEvent || requiresCameraResource(m_eventType)));
                    if (canUseSource)
                        return qnResIconCache->icon(QnResourceIconCache::Camera);
                    break;
                }
                default:
                    break;
            }

            // TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            QnResourceList resources = resourcePool()->getResources(actionResources());
            if (!vms::event::requiresCameraResource(m_actionType))
            {
                return qnResIconCache->icon(QnResourceIconCache::Servers);
            }
            else if (resources.size() == 1)
            {
                QnResourcePtr resource = resources.first();
                return qnResIconCache->icon(resource);
            }
            else if (resources.isEmpty())
            {
                return qnSkin->icon(lit("tree/buggy.png"));
            }
            else
            {
                return qnResIconCache->icon(QnResourceIconCache::Camera);
            }
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
                case vms::event::cameraMotionEvent:
                    return isResourcesListValid<QnCameraMotionPolicy>(
                        resourcePool()->getResources<QnCameraMotionPolicy::resource_type>(filtered));

                case vms::event::cameraInputEvent:
                    return isResourcesListValid<QnCameraInputPolicy>(
                        resourcePool()->getResources<QnCameraInputPolicy::resource_type>(filtered));

                case vms::event::analyticsSdkEvent:
                    return isResourcesListValid<QnCameraAnalyticsPolicy>(
                        resourcePool()->getResources<QnCameraAnalyticsPolicy::resource_type>(filtered));

                case vms::event::softwareTriggerEvent:
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
                                user, Qn::GlobalUserInputPermission);
                        };

                    const auto isRoleValid =
                        [this](const QnUuid& roleId)
                        {
                            const auto role = userRolesManager()->predefinedRole(roleId);
                            switch (role)
                            {
                                case Qn::UserRole::CustomPermissions:
                                    return false;
                                case Qn::UserRole::CustomUserRole:
                                {
                                    const auto customRole = userRolesManager()->userRole(roleId);
                                    return customRole.permissions.testFlag(Qn::GlobalUserInputPermission);
                                }
                                default:
                                {
                                    const auto permissions = userRolesManager()->userRolePermissions(role);
                                    return permissions.testFlag(Qn::GlobalUserInputPermission);
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
            auto filtered = filterActionResources(m_actionResources, m_actionType);
            switch (m_actionType)
            {
                case vms::event::sendMailAction:
                    return QnSendEmailActionDelegate::isValidList(filtered, m_actionParams.emailAddress);
                case vms::event::cameraRecordingAction:
                    return isResourcesListValid<QnCameraRecordingPolicy>(
                        resourcePool()->getResources<QnCameraRecordingPolicy::resource_type>(filtered));
                case vms::event::bookmarkAction:
                    return isResourcesListValid<QnCameraRecordingPolicy>(
                        resourcePool()->getResources<QnBookmarkActionPolicy::resource_type>(filtered));
                case vms::event::cameraOutputAction:
                    return isResourcesListValid<QnCameraOutputPolicy>(
                        resourcePool()->getResources<QnCameraOutputPolicy::resource_type>(filtered));
                case vms::event::playSoundAction:
                case vms::event::playSoundOnceAction:
                    return !m_actionParams.url.isEmpty()
                        && (isResourcesListValid<QnCameraAudioTransmitPolicy>(
                            resourcePool()->getResources<QnCameraAudioTransmitPolicy::resource_type>(filtered))
                            || m_actionParams.playToClient
                        );

                case vms::event::showPopupAction:
                {
                    static const QnDefaultSubjectValidationPolicy defaultPolicy;
                    static const QnRequiredPermissionSubjectPolicy acknowledgePolicy(
                        Qn::GlobalManageBookmarksPermission, QString());

                    const auto subjects = filterSubjectIds(m_actionParams.additionalResources);
                    const auto validationState = m_actionParams.needConfirmation
                        ? acknowledgePolicy.validity(m_actionParams.allUsers, subjects)
                        : defaultPolicy.validity(m_actionParams.allUsers, subjects);

                    return validationState != QValidator::Invalid;
                }

                case vms::event::sayTextAction:
                    return !m_actionParams.sayText.isEmpty()
                        && (isResourcesListValid<QnCameraAudioTransmitPolicy>(
                            resourcePool()->getResources<QnCameraAudioTransmitPolicy::resource_type>(filtered))
                            || m_actionParams.playToClient
                        );

                case vms::event::executePtzPresetAction:
                    return isResourcesListValid<QnExecPtzPresetPolicy>(
                        resourcePool()->getResources<QnExecPtzPresetPolicy::resource_type>(filtered))
                        && m_actionResources.size() == 1
                        && !m_actionParams.presetId.isEmpty();
                case vms::event::showTextOverlayAction:
                case vms::event::showOnAlarmLayoutAction:
                {
                    bool canUseSource = (m_actionParams.useSource
                        && (m_eventType >= vms::event::userDefinedEvent
                            || requiresCameraResource(m_eventType)));
                    if (canUseSource)
                        return true;
                    break;
                }
                case vms::event::execHttpRequestAction:
                {
                    QUrl url(m_actionParams.url);
                    return url.isValid()
                        && !url.isEmpty()
                        && (url.scheme().isEmpty() || url.scheme().toLower() == lit("http"))
                        && !url.host().isEmpty();
                }
                default:
                    break;
            }

            // TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            auto resources = resourcePool()->getResources(filtered);
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
    foreach(vms::event::EventState val, vms::event::allowedEventStates(m_eventType))
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
    bool enableProlongedActions = vms::event::hasToggleState(m_eventType);
    foreach(QModelIndex idx, prolongedActions)
    {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }
}

QString QnBusinessRuleViewModel::getSourceText(const bool detailed) const
{
    QnResourceList resources = resourcePool()->getResources(eventResources());
    if (m_eventType == vms::event::cameraMotionEvent)
        return QnCameraMotionPolicy::getText(resources, detailed);

    if (m_eventType == vms::event::cameraInputEvent)
        return QnCameraInputPolicy::getText(resources, detailed);

    if (m_eventType == vms::event::analyticsSdkEvent)
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

QString QnBusinessRuleViewModel::getTargetText(const bool detailed) const
{
    QnResourceList resources = resourcePool()->getResources(actionResources());
    switch (m_actionType)
    {
        case vms::event::sendMailAction:
        {
            return QnSendEmailActionDelegate::getText(actionResources(),
                detailed,
                m_actionParams.emailAddress);
        }
        case vms::event::showPopupAction:
        {
            if (m_actionParams.allUsers)
                return nx::vms::event::StringsHelper::allUsersText();
            QnUserResourceList users;
            QList<QnUuid> roles;
            userRolesManager()->usersAndRoles(m_actionParams.additionalResources, users, roles);
            users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
            return m_helper->actionSubjects(users, roles);
        }
        case vms::event::bookmarkAction:
        case vms::event::cameraRecordingAction:
        {
            return QnCameraRecordingPolicy::getText(resources, detailed);
        }
        case vms::event::cameraOutputAction:
        {
            return QnCameraOutputPolicy::getText(resources, detailed);
        }

        case vms::event::playSoundAction:
        case vms::event::playSoundOnceAction:
        case vms::event::sayTextAction:
            return QnCameraAudioTransmitPolicy::getText(resources, detailed);

        case vms::event::executePtzPresetAction:
            return QnExecPtzPresetPolicy::getText(resources, detailed);

        case vms::event::showTextOverlayAction:
        case vms::event::showOnAlarmLayoutAction:
        {
            bool canUseSource = (m_actionParams.useSource && (m_eventType >= vms::event::userDefinedEvent || requiresCameraResource(m_eventType)));

            if (canUseSource)
            {
                QnVirtualCameraResourceList targetCameras = resourcePool()->getResources<QnVirtualCameraResource>(m_actionResources);

                if (targetCameras.isEmpty())
                    return tr("Source camera");

                return tr("Source and %n more cameras", "", targetCameras.size());
            }
            break;
        }
        case vms::event::execHttpRequestAction:
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

    const qint64 kMsecPerSec = 1000;
    static const Qt::TimeSpanFormat kFormat = Qt::Seconds | Qt::Minutes | Qt::Hours | Qt::Days;
    static const QString kSeparator(L' ');

    const qint64 aggregationPeriodMs = m_aggregationPeriodSec * kMsecPerSec;
    const QString timespan = QTimeSpan(aggregationPeriodMs).toApproximateString(
        QTimeSpan::kDoNotSuppressSecondUnit,
        kFormat,
        QTimeSpan::SuffixFormat::Full,
        kSeparator);

    return tr("Every %1").arg(timespan);
}

QString QnBusinessRuleViewModel::toggleStateToModelString(vms::event::EventState value)
{
    switch (value)
    {
        case vms::event::EventState::inactive:  return tr("Stops");
        case vms::event::EventState::active:    return tr("Starts");
        case vms::event::EventState::undefined: return tr("Occurs");

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Unknown EventState enumeration value.");
            return QString();
    }
}
