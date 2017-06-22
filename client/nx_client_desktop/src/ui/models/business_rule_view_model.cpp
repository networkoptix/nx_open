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

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <client/client_settings.h>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <business/business_resource_validation.h>
#include <business/business_types_comparator.h>
#include <business/events/abstract_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/motion_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/camera_output_business_action.h>

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

}

namespace QnBusiness {
QList<Columns> allColumns()
{
    static QList<Columns> result;
    if (result.isEmpty())
    {
        result
            << ModifiedColumn
            << DisabledColumn
            << EventColumn
            << SourceColumn
            << SpacerColumn
            << ActionColumn
            << TargetColumn
            << AggregationColumn;
    }
    return result;
}

QSet<QnUuid> toIds(const QnResourceList& resources)
{
    QSet<QnUuid> result;
    for (auto resource : resources)
        result << resource->getId();

    return result;
}

QSet<QnUuid> filterEventResources(const QSet<QnUuid>& ids, EventType eventType)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    if (requiresCameraResource(eventType))
        return toIds(resourcePool->getResources<QnVirtualCameraResource>(ids));

    if (requiresServerResource(eventType))
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

QSet<QnUuid> filterActionResources(const QSet<QnUuid>& ids, ActionType actionType)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    if (requiresCameraResource(actionType))
        return toIds(resourcePool->getResources<QnVirtualCameraResource>(ids));

    if (requiresUserResource(actionType))
        return filterSubjectIds(ids);

    return QSet<QnUuid>();
}

/**
 *  This method must cleanup all action parameters that are not required for the given action.
 *  //TODO: #GDM implement correct filtering
 */
QnBusinessActionParameters filterActionParams(QnBusiness::ActionType actionType, const QnBusinessActionParameters &params)
{
    Q_UNUSED(actionType);
    return params;
}

} //namespace QnBusiness

const QnUuid QnBusinessRuleViewModel::kAllUsersId;

QnBusinessRuleViewModel::QnBusinessRuleViewModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , m_id(QnUuid::createUuid())
    , m_modified(false)
    , m_eventType(QnBusiness::CameraDisconnectEvent)
    , m_eventState(QnBusiness::UndefinedState)
    , m_actionType(QnBusiness::ShowPopupAction)
    , m_aggregationPeriodSec(defaultAggregationPeriodSec)
    , m_disabled(false)
    , m_eventTypesModel(new QStandardItemModel(this))
    , m_eventStatesModel(new QStandardItemModel(this))
    , m_actionTypesModel(new QStandardItemModel(this)),
    m_helper(new QnBusinessStringsHelper(commonModule()))
{

    QnBusinessTypesComparator lexComparator;
    for (QnBusiness::EventType eventType : lexComparator.lexSortedEvents())
    {
        QStandardItem *item = new QStandardItem(m_helper->eventName(eventType));
        item->setData(eventType);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }

    for (QnBusiness::ActionType actionType : lexComparator.lexSortedActions())
    {
        QStandardItem *item = new QStandardItem(m_helper->actionName(actionType));
        item->setData(actionType);
        item->setData(!QnBusiness::canBeInstant(actionType), ProlongedActionRole);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }
    updateActionTypesModel();
    updateEventStateModel();
}

QnBusinessRuleViewModel::~QnBusinessRuleViewModel()
{
}

QVariant QnBusinessRuleViewModel::data(const int column, const int role) const
{
    if (column == QnBusiness::DisabledColumn)
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
                case QnBusiness::EventColumn:
                    return m_eventType;
                case QnBusiness::ActionColumn:
                    return m_actionType;
                case QnBusiness::TargetColumn:
                    if (m_actionType == QnBusiness::SendMailAction)
                        return m_actionParams.emailAddress;
                    break;
                case QnBusiness::AggregationColumn:
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
            auto ids = m_actionType != QnBusiness::ShowPopupAction
                ? filterActionResources(m_actionResources, m_actionType)
                : QnBusiness::filterSubjectIds(m_actionParams.additionalResources);

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

bool QnBusinessRuleViewModel::setData(const int column, const QVariant &value, int role)
{
    if (column == QnBusiness::DisabledColumn && role == Qt::CheckStateRole)
    {
        setDisabled(value.toInt() == Qt::Unchecked);
        return true;
    }

    if (role != Qt::EditRole)
        return false;

    switch (column)
    {
        case QnBusiness::EventColumn:
            setEventType((QnBusiness::EventType)value.toInt());
            return true;
        case QnBusiness::ActionColumn:
            setActionType((QnBusiness::ActionType)value.toInt());
            return true;
        case QnBusiness::SourceColumn:
            setEventResources(value.value<QSet<QnUuid>>());
            return true;
        case QnBusiness::TargetColumn:
        {
            auto subjects = value.value<QSet<QnUuid>>();
            const bool allUsers = subjects.remove(kAllUsersId);

            if (allUsers != m_actionParams.allUsers)
            {
                QnBusinessActionParameters params = m_actionParams;
                params.allUsers = allUsers;
                setActionParams(params);
                if (allUsers)
                    return true;
            }

            switch (m_actionType)
            {
                case QnBusiness::ShowPopupAction:
                {
                    QnBusinessActionParameters params = m_actionParams;
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
        case QnBusiness::AggregationColumn:
            setAggregationPeriod(value.toInt());
            return true;
        default:
            break;
    }

    return false;
}


void QnBusinessRuleViewModel::loadFromRule(QnBusinessEventRulePtr businessRule)
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

    updateActionTypesModel();//TODO: #GDM #Business connect on dataChanged?

    emit dataChanged(QnBusiness::AllFieldsMask);
}

QnBusinessEventRulePtr QnBusinessRuleViewModel::createRule() const
{
    QnBusinessEventRulePtr rule(new QnBusinessEventRule());
    rule->setId(m_id);
    rule->setEventType(m_eventType);
    rule->setEventResources(toIdList(filterEventResources(m_eventResources, m_eventType)));
    rule->setEventState(m_eventState);   //TODO: #GDM #Business check
    rule->setEventParams(m_eventParams); //TODO: #GDM #Business filtered
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
    emit dataChanged(QnBusiness::ModifiedField);
}

QnBusiness::EventType QnBusinessRuleViewModel::eventType() const
{
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const QnBusiness::EventType value)
{
    if (m_eventType == value)
        return;

    bool cameraRequired = QnBusiness::requiresCameraResource(m_eventType);
    bool serverRequired = QnBusiness::requiresServerResource(m_eventType);

    m_eventType = value;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::EventTypeField | QnBusiness::ModifiedField;

    if (QnBusiness::requiresCameraResource(m_eventType) != cameraRequired ||
        QnBusiness::requiresServerResource(m_eventType) != serverRequired)
    {
        fields |= QnBusiness::EventResourcesField;
    }

    if (QnBusiness::hasToggleState(m_eventType))
    {
        if (!isActionProlonged())
        {
            const auto allowedStates = allowedEventStates(m_eventType);
            if (!allowedStates.contains(m_eventState))
            {
                m_eventState = allowedStates.first();
                fields |= QnBusiness::EventStateField;
            }
        }
    }
    else
    {
        if (m_eventState != QnBusiness::UndefinedState)
        {
            m_eventState = QnBusiness::UndefinedState;
            fields |= QnBusiness::EventStateField;
        }

        if (!QnBusiness::canBeInstant(m_actionType))
        {
            m_actionType = QnBusiness::ShowPopupAction;
            fields |= QnBusiness::ActionTypeField | QnBusiness::ActionResourcesField | QnBusiness::ActionParamsField;
        }
        else if (isActionProlonged() && QnBusiness::supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
        {
            m_actionParams.durationMs = defaultActionDurationMs;
            fields |= QnBusiness::ActionParamsField;
        }
    }

    switch (m_eventType)
    {
        case QnBusiness::CameraInputEvent:
            m_eventParams.inputPortId = QString();
            break;

        case QnBusiness::SoftwareTriggerEvent:
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
    //TODO: #GDM #Business check others, params and resources should be merged
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

    emit dataChanged(QnBusiness::EventResourcesField | QnBusiness::ModifiedField);
}

QnBusinessEventParameters QnBusinessRuleViewModel::eventParams() const
{
    return m_eventParams;
}

void QnBusinessRuleViewModel::setEventParams(const QnBusinessEventParameters &params)
{
    bool hasChanges = !(m_eventParams == params);

    if (!hasChanges)
        return;

    m_eventParams = params;
    m_modified = true;

    emit dataChanged(QnBusiness::EventParamsField | QnBusiness::ModifiedField);
}

QnBusiness::EventState QnBusinessRuleViewModel::eventState() const
{
    return m_eventState;
}

void QnBusinessRuleViewModel::setEventState(QnBusiness::EventState state)
{
    if (m_eventState == state)
        return;

    m_eventState = state;
    m_modified = true;

    emit dataChanged(QnBusiness::EventStateField | QnBusiness::ModifiedField);
}

QnBusiness::ActionType QnBusinessRuleViewModel::actionType() const
{
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const QnBusiness::ActionType value)
{
    if (m_actionType == value)
        return;

    bool cameraRequired = QnBusiness::requiresCameraResource(m_actionType);
    bool userRequired = QnBusiness::requiresUserResource(m_actionType);

    bool wasEmailAction = m_actionType == QnBusiness::SendMailAction;
    bool aggregationWasDisabled = !QnBusiness::allowsAggregation(m_actionType);

    m_actionType = value;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::ActionTypeField | QnBusiness::ModifiedField;
    if (QnBusiness::requiresCameraResource(m_actionType) != cameraRequired ||
        QnBusiness::requiresUserResource(m_actionType) != userRequired)
        fields |= QnBusiness::ActionResourcesField;

    if (!QnBusiness::allowsAggregation(m_actionType))
    {
        m_aggregationPeriodSec = 0;
        fields |= QnBusiness::AggregationField;
    }
    else
    {
        /*
         *  If action is "send email" default units for aggregation period should be hours, not minutes.
         *  Works only if aggregation period was not changed from default value.
         */
        if (value == QnBusiness::SendMailAction && m_aggregationPeriodSec == defaultAggregationPeriodSec)
        {
            m_aggregationPeriodSec = defaultAggregationPeriodSec * 60;
            fields |= QnBusiness::AggregationField;
        }
        else if (wasEmailAction && m_aggregationPeriodSec == defaultAggregationPeriodSec * 60)
        {
            m_aggregationPeriodSec = defaultAggregationPeriodSec;
            fields |= QnBusiness::AggregationField;
        }
        else if (aggregationWasDisabled)
        {
            m_aggregationPeriodSec = defaultAggregationPeriodSec;
            fields |= QnBusiness::AggregationField;
        }
    }

    if (QnBusiness::hasToggleState(m_eventType) && !isActionProlonged() && m_eventState == QnBusiness::UndefinedState)
    {
        m_eventState = allowedEventStates(m_eventType).first();
        fields |= QnBusiness::EventStateField;
    }
    else if (!QnBusiness::hasToggleState(m_eventType) && QnBusiness::supportsDuration(m_actionType) && m_actionParams.durationMs <= 0)
    {
        m_actionParams.durationMs = defaultActionDurationMs;
        fields |= QnBusiness::ActionParamsField;
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

    emit dataChanged(QnBusiness::ActionResourcesField | QnBusiness::ModifiedField);
}

bool QnBusinessRuleViewModel::isActionProlonged() const
{
    return QnBusiness::isActionProlonged(m_actionType, m_actionParams);
}

QnBusinessActionParameters QnBusinessRuleViewModel::actionParams() const
{
    return m_actionParams;
}

void QnBusinessRuleViewModel::setActionParams(const QnBusinessActionParameters &params)
{
    if (params == m_actionParams)
        return;

    m_actionParams = params;
    m_modified = true;

    emit dataChanged(QnBusiness::ActionParamsField | QnBusiness::ModifiedField);
}

int QnBusinessRuleViewModel::aggregationPeriod() const
{
    return m_aggregationPeriodSec;
}

void QnBusinessRuleViewModel::setAggregationPeriod(int msecs)
{
    if (m_aggregationPeriodSec == msecs)
        return;

    m_aggregationPeriodSec = msecs;
    m_modified = true;

    emit dataChanged(QnBusiness::AggregationField | QnBusiness::ModifiedField);
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

    emit dataChanged(QnBusiness::AllFieldsMask); // all fields should be redrawn
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

    emit dataChanged(QnBusiness::CommentsField | QnBusiness::ModifiedField);
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

    emit dataChanged(QnBusiness::ScheduleField | QnBusiness::ModifiedField);
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

QString QnBusinessRuleViewModel::getText(const int column, const bool detailed) const
{
    switch (column)
    {
        case QnBusiness::ModifiedColumn:
            return (m_modified ? QLatin1String("*") : QString());
        case QnBusiness::EventColumn:
            return m_helper->eventTypeString(m_eventType, m_eventState, m_actionType, m_actionParams);
        case QnBusiness::SourceColumn:
            return getSourceText(detailed);
        case QnBusiness::SpacerColumn:
            return QString();
        case QnBusiness::ActionColumn:
            return m_helper->actionName(m_actionType);
        case QnBusiness::TargetColumn:
            return getTargetText(detailed);
        case QnBusiness::AggregationColumn:
            return getAggregationText();
        default:
            break;
    }
    return QString();
}

QString QnBusinessRuleViewModel::getToolTip(const int column) const
{
    if (isValid())
        return m_comments.isEmpty() ? getText(column) : m_comments;

    const QString errorMessage = tr("Error: %1");

    switch (column)
    {
        case QnBusiness::ModifiedColumn:
        case QnBusiness::EventColumn:
        case QnBusiness::SourceColumn:
            if (!isValid(QnBusiness::SourceColumn))
                return errorMessage.arg(getText(QnBusiness::SourceColumn));
            return errorMessage.arg(getText(QnBusiness::TargetColumn));
        default:
            // at least one of them should be invalid
            if (!isValid(QnBusiness::TargetColumn))
                return errorMessage.arg(getText(QnBusiness::TargetColumn));
            return errorMessage.arg(getText(QnBusiness::SourceColumn));
    }
    return QString();
}

QIcon QnBusinessRuleViewModel::getIcon(const int column) const
{
    switch (column)
    {
        case QnBusiness::SourceColumn:
        {
            //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            auto resources = resourcePool()->getResources(eventResources());
            if (!QnBusiness::isResourceRequired(m_eventType))
            {
                return qnResIconCache->icon(QnResourceIconCache::CurrentSystem);
            }
            else if (resources.size() == 1)
            {
                QnResourcePtr resource = resources.first();
                return qnResIconCache->icon(resource);
            }
            else if (QnBusiness::requiresServerResource(m_eventType))
            {
                return qnResIconCache->icon(QnResourceIconCache::Server);
            }
            else /* ::requiresCameraResource(m_eventType) */
            {
                return qnResIconCache->icon(QnResourceIconCache::Camera);
            }
        }
        case QnBusiness::TargetColumn:
        {
            switch (m_actionType)
            {
                case QnBusiness::SendMailAction:
                {
                    if (!isValid(QnBusiness::TargetColumn))
                        return qnSkin->icon("tree/user_alert.png");
                    return qnResIconCache->icon(QnResourceIconCache::Users);
                }

                case QnBusiness::ShowPopupAction:
                {
                    if (m_actionParams.allUsers)
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                    if (!isValid(QnBusiness::TargetColumn))
                        return qnSkin->icon("tree/user_alert.png");

                    QnUserResourceList users;
                    QList<QnUuid> roles;
                    userRolesManager()->usersAndRoles(m_actionParams.additionalResources, users, roles);
                    return (users.size() > 1 || !roles.empty())
                        ? qnResIconCache->icon(QnResourceIconCache::Users)
                        : qnResIconCache->icon(QnResourceIconCache::User);
                }

                case QnBusiness::ShowTextOverlayAction:
                case QnBusiness::ShowOnAlarmLayoutAction:
                {
                    bool canUseSource = (m_actionParams.useSource && (m_eventType >= QnBusiness::UserDefinedEvent || requiresCameraResource(m_eventType)));
                    if (canUseSource)
                        return qnResIconCache->icon(QnResourceIconCache::Camera);
                    break;
                }
                default:
                    break;
            }

            //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            QnResourceList resources = resourcePool()->getResources(actionResources());
            if (!QnBusiness::requiresCameraResource(m_actionType))
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

int QnBusinessRuleViewModel::getHelpTopic(const int column) const
{
    switch (column)
    {
        case QnBusiness::EventColumn:
            return QnBusiness::eventHelpId(m_eventType);
        case QnBusiness::ActionColumn:
            return QnBusiness::actionHelpId(m_actionType);
        default:
            return Qn::EventsActions_Help;
    }
}

bool QnBusinessRuleViewModel::isValid() const
{
    return m_disabled || (isValid(QnBusiness::SourceColumn) && isValid(QnBusiness::TargetColumn));
}

bool QnBusinessRuleViewModel::isValid(int column) const
{
    switch (column)
    {
        //TODO: #vkutin Add Software Triggers support

        case QnBusiness::SourceColumn:
        {
            auto filtered = filterEventResources(m_eventResources, m_eventType);
            switch (m_eventType)
            {
                case QnBusiness::CameraMotionEvent:
                    return isResourcesListValid<QnCameraMotionPolicy>(
                        resourcePool()->getResources<QnCameraMotionPolicy::resource_type>(filtered));
                case QnBusiness::CameraInputEvent:
                    return isResourcesListValid<QnCameraInputPolicy>(
                        resourcePool()->getResources<QnCameraInputPolicy::resource_type>(filtered));
                default:
                    return true;
            }
        }
        case QnBusiness::TargetColumn:
        {
            auto filtered = filterActionResources(m_actionResources, m_actionType);
            switch (m_actionType)
            {
                case QnBusiness::SendMailAction:
                    return QnSendEmailActionDelegate::isValidList(filtered, m_actionParams.emailAddress);
                case QnBusiness::CameraRecordingAction:
                    return isResourcesListValid<QnCameraRecordingPolicy>(
                        resourcePool()->getResources<QnCameraRecordingPolicy::resource_type>(filtered));
                case QnBusiness::BookmarkAction:
                    return isResourcesListValid<QnCameraRecordingPolicy>(
                        resourcePool()->getResources<QnBookmarkActionPolicy::resource_type>(filtered));
                case QnBusiness::CameraOutputAction:
                    return isResourcesListValid<QnCameraOutputPolicy>(
                        resourcePool()->getResources<QnCameraOutputPolicy::resource_type>(filtered));
                case QnBusiness::PlaySoundAction:
                case QnBusiness::PlaySoundOnceAction:
		            return !m_actionParams.url.isEmpty()
		                && (isResourcesListValid<QnCameraAudioTransmitPolicy>(
		                    resourcePool()->getResources<QnCameraAudioTransmitPolicy::resource_type>(filtered))
		                    || m_actionParams.playToClient
		                );

                case QnBusiness::ShowPopupAction:
                    return m_actionParams.allUsers || !QnBusiness::filterSubjectIds(
                        m_actionParams.additionalResources).empty();

                case QnBusiness::SayTextAction:
		            return !m_actionParams.sayText.isEmpty()
		                && (isResourcesListValid<QnCameraAudioTransmitPolicy>(
		                    resourcePool()->getResources<QnCameraAudioTransmitPolicy::resource_type>(filtered))
		                    || m_actionParams.playToClient
		                );

                case QnBusiness::ExecutePtzPresetAction:
                    return isResourcesListValid<QnExecPtzPresetPolicy>(
                        resourcePool()->getResources<QnExecPtzPresetPolicy::resource_type>(filtered))
                        && m_actionResources.size() == 1
                        && !m_actionParams.presetId.isEmpty();
                case QnBusiness::ShowTextOverlayAction:
                case QnBusiness::ShowOnAlarmLayoutAction:
                {
                    bool canUseSource = (m_actionParams.useSource
                        && (m_eventType >= QnBusiness::UserDefinedEvent
                            || requiresCameraResource(m_eventType)));
                    if (canUseSource)
                        return true;
                    break;
                }
                case QnBusiness::ExecHttpRequestAction:
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

            //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
            auto resources = resourcePool()->getResources(filtered);
            if (QnBusiness::requiresCameraResource(m_actionType) && resources.isEmpty())
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
    foreach(QnBusiness::EventState val, QnBusiness::allowedEventStates(m_eventType))
    {
        QStandardItem *item = new QStandardItem(toggleStateToModelString(val));
        item->setData(val);

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
    bool enableProlongedActions = QnBusiness::hasToggleState(m_eventType);
    foreach(QModelIndex idx, prolongedActions)
    {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }
}

QString QnBusinessRuleViewModel::getSourceText(const bool detailed) const
{
    QnResourceList resources = resourcePool()->getResources(eventResources());
    if (m_eventType == QnBusiness::CameraMotionEvent)
        return QnCameraMotionPolicy::getText(resources, detailed);

    if (m_eventType == QnBusiness::CameraInputEvent)
        return QnCameraInputPolicy::getText(resources, detailed);

    if (!QnBusiness::isResourceRequired(m_eventType))
        return braced(tr("System"));

    if (resources.size() == 1)
        return QnResourceDisplayInfo(resources.first()).toString(qnSettings->extraInfoInTree());

    if (QnBusiness::requiresServerResource(m_eventType))
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
        case QnBusiness::SendMailAction:
        {
            return QnSendEmailActionDelegate::getText(actionResources(),
                detailed,
                m_actionParams.emailAddress);
        }
        case QnBusiness::ShowPopupAction:
        {
            if (m_actionParams.allUsers)
                return QnBusinessStringsHelper::allUsersText();
            QnUserResourceList users;
            QList<QnUuid> roles;
            userRolesManager()->usersAndRoles(m_actionParams.additionalResources, users, roles);
            return m_helper->actionSubjects(users, roles);
        }
        case QnBusiness::BookmarkAction:
        case QnBusiness::CameraRecordingAction:
        {
            return QnCameraRecordingPolicy::getText(resources, detailed);
        }
        case QnBusiness::CameraOutputAction:
        {
            return QnCameraOutputPolicy::getText(resources, detailed);
        }

        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        case QnBusiness::SayTextAction:
            return QnCameraAudioTransmitPolicy::getText(resources, detailed);

        case QnBusiness::ExecutePtzPresetAction:
            return QnExecPtzPresetPolicy::getText(resources, detailed);

        case QnBusiness::ShowTextOverlayAction:
        case QnBusiness::ShowOnAlarmLayoutAction:
        {
            bool canUseSource = (m_actionParams.useSource && (m_eventType >= QnBusiness::UserDefinedEvent || requiresCameraResource(m_eventType)));

            if (canUseSource)
            {
                QnVirtualCameraResourceList targetCameras = resourcePool()->getResources<QnVirtualCameraResource>(m_actionResources);

                if (targetCameras.isEmpty())
                    return tr("Source camera");

                return tr("Source and %n more cameras", "", targetCameras.size());
            }
            break;
        }
        case QnBusiness::ExecHttpRequestAction:
            return QUrl(m_actionParams.url).toString(QUrl::RemoveUserInfo);
        default:
            break;
    }

    //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
    if (!QnBusiness::requiresCameraResource(m_actionType))
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
    if (!QnBusiness::allowsAggregation(m_actionType))
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

QString QnBusinessRuleViewModel::toggleStateToModelString(QnBusiness::EventState value)
{
    switch (value)
    {
        case QnBusiness::InactiveState:
            return tr("Stops");
        case QnBusiness::ActiveState:
            return tr("Starts");
        case QnBusiness::UndefinedState:
            return tr("Occurs");
    }
    return QString();
}
