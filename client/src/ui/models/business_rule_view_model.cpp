#include "business_rule_view_model.h"

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <business/business_resource_validation.h>
#include <business/events/abstract_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/motion_business_event.h>
#include <business/actions/abstract_business_action.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/camera_output_business_action.h>

#include <ui/help/help_topics.h>
#include <ui/help/business_help.h>
#include <ui/common/ui_resource_name.h>
#include <ui/models/notification_sound_model.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>
#include "core/resource_management/resource_pool.h"

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;
}

namespace QnBusiness {
    QList<Columns> allColumns() {
        static QList<Columns> result;
        if (result.isEmpty()) {
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

    template <class T>
    QnSharedResourcePointerList<T> filteredResources(const IDList &idList)
    {
        return qnResPool->getResources<T>(idList);
    }

    QnResourceList filterEventResources(const IDList &resources, EventType eventType) {
        if (requiresCameraResource(eventType))
            return filteredResources<QnVirtualCameraResource>(resources);
        if (requiresServerResource(eventType))
            return filteredResources<QnMediaServerResource>(resources);
        return QnResourceList();
    }

    QnResourceList filterActionResources(const IDList &resources, ActionType actionType) {
        if (requiresCameraResource(actionType))
            return filteredResources<QnVirtualCameraResource>(resources);
        if (requiresUserResource(actionType))
            return filteredResources<QnUserResource>(resources);
        return QnResourceList();
    }
}


QnBusinessRuleViewModel::QnBusinessRuleViewModel(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_modified(false),
    m_eventType(QnBusiness::CameraDisconnectEvent),
    m_eventState(QnBusiness::UndefinedState),
    m_actionType(QnBusiness::ShowPopupAction),
    m_aggregationPeriod(60),
    m_disabled(false),
    m_system(false),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this))
{

    for (QnBusiness::EventType eventType: QnBusiness::allEvents()) {
        QStandardItem *item = new QStandardItem(QnBusinessStringsHelper::eventName(eventType));
        item->setData(eventType);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }

    for (QnBusiness::ActionType actionType: QnBusiness::allActions()) {      
        QStandardItem *item = new QStandardItem(QnBusinessStringsHelper::actionName(actionType));
        item->setData(actionType);
        item->setData(QnBusiness::hasToggleState(actionType), ProlongedActionRole);
        
        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }
    updateActionTypesModel();
    updateEventStateModel();
}

QnBusinessRuleViewModel::~QnBusinessRuleViewModel() {

}

QVariant QnBusinessRuleViewModel::data(const int column, const int role) const {
    if (column == QnBusiness::DisabledColumn) {
        switch (role) {
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

    switch (role) {
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
        switch (column) {
        case QnBusiness::EventColumn:
            return m_eventType;
        case QnBusiness::ActionColumn:
            return m_actionType;
        case QnBusiness::TargetColumn:
        {
            switch (m_actionType) {
            case QnBusiness::SendMailAction:
                return m_actionParams.emailAddress;
            case QnBusiness::ShowPopupAction:
                return (int)m_actionParams.userGroup;
            case QnBusiness::PlaySoundAction:
            case QnBusiness::PlaySoundOnceAction:
                return m_actionParams.soundUrl;
            case QnBusiness::SayTextAction:
                return m_actionParams.sayText;
            default:
                break;
            }
        }
        case QnBusiness::AggregationColumn:
            return m_aggregationPeriod;
        default:
            break;
        }

    case Qt::TextColorRole:
        break;

    case Qt::BackgroundRole:
        if (m_system || m_disabled || isValid())
            break;

        if (!isValid(column))
            return QBrush(qnGlobals->businessRuleInvalidColumnBackgroundColor());
        return QBrush(qnGlobals->businessRuleInvalidBackgroundColor());

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
        return QVariant::fromValue<QnResourceList>(filterEventResources(m_eventResources, m_eventType));
    case Qn::ActionTypeRole:
        return qVariantFromValue(m_actionType);
    case Qn::ActionResourcesRole:
        return QVariant::fromValue<QnResourceList>(filterActionResources(m_actionResources, m_actionType));

    case Qn::HelpTopicIdRole:
        return getHelpTopic(column);
    default:
        break;
    }
    return QVariant();
}

bool QnBusinessRuleViewModel::setData(const int column, const QVariant &value, int role) {
    if (m_system)
        return false;

    if (column == QnBusiness::DisabledColumn && role == Qt::CheckStateRole) {
        Qt::CheckState checked = (Qt::CheckState)value.toInt();
        setDisabled(checked == Qt::Unchecked);
        return true;
    }

    if (role != Qt::EditRole)
        return false;

    switch (column) {
    case QnBusiness::EventColumn:
        setEventType((QnBusiness::EventType)value.toInt());
        return true;
    case QnBusiness::ActionColumn:
        setActionType((QnBusiness::ActionType)value.toInt());
        return true;
    case QnBusiness::SourceColumn:
        setEventResources(filterEventResources(value.value<IDList>(), m_eventType));
        return true;
    case QnBusiness::TargetColumn:
        switch(m_actionType) {
        case QnBusiness::ShowPopupAction:
        {
            QnBusinessActionParameters params = m_actionParams;

            // TODO: #GDM #Business you're implicitly relying on what enum values are, which is very bad.
            // This code will fail silently if someone changes the header. Please write it properly.           
            params.userGroup = (QnBusiness::UserGroup)value.toInt(); 
            setActionParams(params);
            break;
        }
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        {
            QnBusinessActionParameters params;
            params.soundUrl = value.toString();
            setActionParams(params);
            break;
        }
        case QnBusiness::SayTextAction:
        {
            QnBusinessActionParameters params;
            params.sayText = value.toString();
            setActionParams(params);
            break;
        }
        default:
            setActionResources(filterActionResources(value.value<IDList>(), m_actionType));
            break;
        }
        return true;
    case QnBusiness::AggregationColumn:
        setAggregationPeriod(value.toInt());
        return true;
    default:
        break;
    }

    return false;
}


void QnBusinessRuleViewModel::loadFromRule(QnBusinessEventRulePtr businessRule) {
    m_id = businessRule->id();
    m_modified = false;
    if (m_eventType != businessRule->eventType()) {
        m_eventType = businessRule->eventType();
        updateEventStateModel();
    }

    m_eventResources = businessRule->eventResources();

    m_eventParams = businessRule->eventParams();

    m_eventState = businessRule->eventState();

    m_actionType = businessRule->actionType();

    m_actionResources = businessRule->actionResources();

    m_actionParams = businessRule->actionParams();

    m_aggregationPeriod = businessRule->aggregationPeriod();

    m_disabled = businessRule->isDisabled();
    m_comments = businessRule->comment();
    m_schedule = businessRule->schedule();
    m_system = businessRule->isSystem();

    updateActionTypesModel();//TODO: #GDM #Business connect on dataChanged?

    emit dataChanged(this, QnBusiness::AllFieldsMask);
}

static QVector<QnUuid> toIdList(const QnResourceList& list)
{
    QVector<QnUuid> result;
    result.reserve(list.size());
    for (int i = 0; i < list.size(); ++i)
        result << list[i]->getId();
    return result;
}

QnBusinessEventRulePtr QnBusinessRuleViewModel::createRule() const {
    QnBusinessEventRulePtr rule(new QnBusinessEventRule());
    rule->setId(m_id);
    rule->setEventType(m_eventType);
    rule->setEventResources(toIdList(filterEventResources(m_eventResources, m_eventType)));
    rule->setEventState(m_eventState);   //TODO: #GDM #Business check
    rule->setEventParams(m_eventParams); //TODO: #GDM #Business filtered
    rule->setActionType(m_actionType);
    rule->setActionResources(toIdList(filterActionResources(m_actionResources, m_actionType)));
    rule->setActionParams(m_actionParams); //TODO: #GDM #Business filtered
    rule->setAggregationPeriod(m_aggregationPeriod);
    rule->setDisabled(m_disabled);
    rule->setComment(m_comments);
    rule->setSchedule(m_schedule);
    return rule;
}

// setters and getters


QnUuid QnBusinessRuleViewModel::id() const {
    return m_id;
}

bool QnBusinessRuleViewModel::isModified() const {
    return m_modified;
}

void QnBusinessRuleViewModel::setModified(bool value) {
    if (m_modified == value)
        return;

    m_modified = value;
    emit dataChanged(this, QnBusiness::ModifiedField);
}

QnBusiness::EventType QnBusinessRuleViewModel::eventType() const {
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const QnBusiness::EventType value) {
    if (m_eventType == value)
        return;

    bool cameraRequired = QnBusiness::requiresCameraResource(m_eventType);
    bool serverRequired = QnBusiness::requiresServerResource(m_eventType);

    m_eventType = value;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::EventTypeField | QnBusiness::ModifiedField;

    if (QnBusiness::requiresCameraResource(m_eventType) != cameraRequired ||
            QnBusiness::requiresServerResource(m_eventType) != serverRequired) {
        fields |= QnBusiness::EventResourcesField;
    }

    if (!QnBusiness::hasToggleState(m_eventType) && m_eventState != QnBusiness::UndefinedState){
        m_eventState = QnBusiness::UndefinedState;
        fields |= QnBusiness::EventStateField;
    } else if (QnBusiness::hasToggleState(m_eventType) && !QnBusiness::hasToggleState(m_actionType)) {
        m_eventState = allowedEventStates(m_eventType).first();
        fields |= QnBusiness::EventStateField;
    }
    if (!QnBusiness::hasToggleState(m_eventType) && QnBusiness::hasToggleState(m_actionType)) {
        m_actionType = QnBusiness::ShowPopupAction;
        fields |= QnBusiness::ActionTypeField | QnBusiness::ActionResourcesField | QnBusiness::ActionParamsField;
    }

    updateActionTypesModel();
    updateEventStateModel();

    emit dataChanged(this, fields);
    //TODO: #GDM #Business check others, params and resources should be merged
}


QnResourceList QnBusinessRuleViewModel::eventResources() const {
    return filterEventResources(m_eventResources, eventType());
}

void QnBusinessRuleViewModel::setEventResources(const QnResourceList &value) {
    IDList newValue;
    for(const auto& r: value)
        newValue << r->getId();
    if (newValue == m_eventResources)
        return; //TODO: #GDM #Business check equal

    m_eventResources = newValue;
    m_modified = true;

    emit dataChanged(this, QnBusiness::EventResourcesField | QnBusiness::ModifiedField);
}

QnBusinessEventParameters QnBusinessRuleViewModel::eventParams() const {
    return m_eventParams;
}

void QnBusinessRuleViewModel::setEventParams(const QnBusinessEventParameters &params)
{
    bool hasChanges = !(m_eventParams == params);
    /*
    bool hasChanges = false;
    for (int i = 0; i < (int) params.CountParam; ++i)
    {
        //if (m_eventParams[i] == params[i])
        //    continue;
        m_eventParams[i] = params[i];
        hasChanges = true;
    }
    */

    if (!hasChanges)
        return;

    m_eventParams = params;
    m_modified = true;

    emit dataChanged(this, QnBusiness::EventParamsField | QnBusiness::ModifiedField);
}

QnBusiness::EventState QnBusinessRuleViewModel::eventState() const {
    return m_eventState;
}

void QnBusinessRuleViewModel::setEventState(QnBusiness::EventState state) {
    if (m_eventState == state)
        return;

    m_eventState = state;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::EventStateField | QnBusiness::ModifiedField;
    emit dataChanged(this, fields);
}

QnBusiness::ActionType QnBusinessRuleViewModel::actionType() const {
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const QnBusiness::ActionType value) {
    if (m_actionType == value)
        return;

    bool cameraRequired = QnBusiness::requiresCameraResource(m_actionType);
    bool userRequired = QnBusiness::requiresUserResource(m_actionType);

    bool wasEmailAction = m_actionType == QnBusiness::SendMailAction;

    m_actionType = value;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::ActionTypeField | QnBusiness::ModifiedField;
    if (QnBusiness::requiresCameraResource(m_actionType) != cameraRequired ||
            QnBusiness::requiresUserResource(m_actionType) != userRequired)
        fields |= QnBusiness::ActionResourcesField;

    /*
     *  If action is "send email" default units for aggregation period should be hours, not minutes.
     *  Works only if aggregation period was not changed from default value.
     */
    if (value == QnBusiness::SendMailAction && m_aggregationPeriod == 60) {
        m_aggregationPeriod = 60*60;
        fields |= QnBusiness::AggregationField;
    } else if (wasEmailAction && m_aggregationPeriod == 60*60) {
        m_aggregationPeriod = 60;
        fields |= QnBusiness::AggregationField;
    }

    if (QnBusiness::hasToggleState(m_eventType) && !isActionProlonged() && m_eventState == QnBusiness::UndefinedState) {
        m_eventState = allowedEventStates(m_eventType).first();
        fields |= QnBusiness::EventStateField;
    }

    emit dataChanged(this, fields);
}

QnResourceList QnBusinessRuleViewModel::actionResources() const {
    return filterActionResources(m_actionResources, actionType());
}

void QnBusinessRuleViewModel::setActionResources(const QnResourceList &value) {
    IDList newValue;
    for(const auto& r: value)
        newValue << r->getId();
    if (newValue == m_actionResources)
        return;

    m_actionResources = newValue;
    m_modified = true;

    emit dataChanged(this, QnBusiness::ActionResourcesField | QnBusiness::ModifiedField);
}

bool QnBusinessRuleViewModel::isActionProlonged() const {
    return QnBusiness::isActionProlonged(m_actionType, m_actionParams);
}

QnBusinessActionParameters QnBusinessRuleViewModel::actionParams() const
{
    return m_actionParams;
}

void QnBusinessRuleViewModel::setActionParams(const QnBusinessActionParameters &params) {
    if (params == m_actionParams)
        return;

    m_actionParams = params;
    m_modified = true;

    emit dataChanged(this, QnBusiness::ActionParamsField | QnBusiness::ModifiedField);
}

int QnBusinessRuleViewModel::aggregationPeriod() const {
    return m_aggregationPeriod;
}

void QnBusinessRuleViewModel::setAggregationPeriod(int msecs) {
    if (m_aggregationPeriod == msecs)
        return;

    m_aggregationPeriod = msecs;
    m_modified = true;

    emit dataChanged(this, QnBusiness::AggregationField | QnBusiness::ModifiedField);
}

bool QnBusinessRuleViewModel::disabled() const {
    return m_disabled;
}

void QnBusinessRuleViewModel::setDisabled(const bool value) {
    if (m_disabled == value)
        return;

    m_disabled = value;
    m_modified = true;

    emit dataChanged(this, QnBusiness::AllFieldsMask); // all fields should be redrawn
}

bool QnBusinessRuleViewModel::system() const {
    return m_system;
}

QString QnBusinessRuleViewModel::comments() const {
    return m_comments;
}

void QnBusinessRuleViewModel::setComments(const QString value) {
    if (m_comments == value)
        return;

    m_comments = value;
    m_modified = true;

    emit dataChanged(this, QnBusiness::CommentsField | QnBusiness::ModifiedField);
}

QString QnBusinessRuleViewModel::schedule() const {
    return m_schedule;
}

void QnBusinessRuleViewModel::setSchedule(const QString value) {
    if (m_schedule == value)
        return;

    m_schedule = value;
    m_modified = true;

    emit dataChanged(this, QnBusiness::ScheduleField | QnBusiness::ModifiedField);
}

QStandardItemModel* QnBusinessRuleViewModel::eventTypesModel() {
    return m_eventTypesModel;
}

QStandardItemModel* QnBusinessRuleViewModel::eventStatesModel() {
    return m_eventStatesModel;
}

QStandardItemModel* QnBusinessRuleViewModel::actionTypesModel() {
    return m_actionTypesModel;
}

// utilities

QString QnBusinessRuleViewModel::getText(const int column, const bool detailed) const {
    switch (column) {
    case QnBusiness::ModifiedColumn:
        return (m_modified ? QLatin1String("*") : QString());
    case QnBusiness::EventColumn:
        return QnBusinessStringsHelper::eventTypeString(m_eventType, m_eventState, m_actionType, m_actionParams);
    case QnBusiness::SourceColumn:
        return getSourceText(detailed);
    case QnBusiness::SpacerColumn:
        return QString();
    case QnBusiness::ActionColumn:
        return QnBusinessStringsHelper::actionName(m_actionType);
    case QnBusiness::TargetColumn:
        return getTargetText(detailed);
    case QnBusiness::AggregationColumn:
        return getAggregationText();
    default:
        break;
    }
    return QString();
}

QString QnBusinessRuleViewModel::getToolTip(const int column) const {
    if (isValid())
        return m_comments.isEmpty() ? getText(column) : m_comments;

    const QString errorMessage = tr("Error: %1");

    switch (column) {
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

QIcon QnBusinessRuleViewModel::getIcon(const int column) const {
    switch (column) {
    case QnBusiness::SourceColumn:
    {
        //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
        QnResourceList resources = filterEventResources(m_eventResources, m_eventType);
        if (!QnBusiness::isResourceRequired(m_eventType)) {
            return qnResIconCache->icon(QnResourceIconCache::Servers);
        } else if (resources.size() == 1) {
            QnResourcePtr resource = resources.first();
            return qnResIconCache->icon(resource);
        } else if (QnBusiness::requiresServerResource(m_eventType)){
            return qnResIconCache->icon(QnResourceIconCache::Server);
        } else /* ::requiresCameraResource(m_eventType) */ {
            return qnResIconCache->icon(QnResourceIconCache::Camera);
        }
    }
    case QnBusiness::TargetColumn:
    {
        switch (m_actionType) {
        case QnBusiness::SendMailAction:
        {
            if (!isValid(QnBusiness::TargetColumn))
                return qnResIconCache->icon(QnResourceIconCache::Offline, true);
            else
                return qnResIconCache->icon(QnResourceIconCache::Users);
        }
        case QnBusiness::ShowPopupAction:
        {
            if (m_actionParams.userGroup == QnBusiness::AdminOnly)
                return qnResIconCache->icon(QnResourceIconCache::User);
            else
                return qnResIconCache->icon(QnResourceIconCache::Users);
        }
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        case QnBusiness::SayTextAction:
            return qnSkin->icon("events/sound.png");
        default:
            break;
        }

        //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
        QnResourceList resources = filterActionResources(m_actionResources, m_actionType);
        if (!QnBusiness::requiresCameraResource(m_actionType)) {
            return qnResIconCache->icon(QnResourceIconCache::Servers);
        } else if (resources.size() == 1) {
            QnResourcePtr resource = resources.first();
            return qnResIconCache->icon(resource);
        } else if (resources.isEmpty()) {
            return qnResIconCache->icon(QnResourceIconCache::Offline, true);
        } else {
            return qnResIconCache->icon(QnResourceIconCache::Camera);
        }
    }
    default:
        break;
    }
    return QIcon();
}

int QnBusinessRuleViewModel::getHelpTopic(const int column) const {
    switch (column) {
    case QnBusiness::EventColumn:
        return QnBusiness::eventHelpId(m_eventType);
    case QnBusiness::ActionColumn:
        return QnBusiness::actionHelpId(m_actionType);
    default:
        return Qn::EventsActions_Help;
    }
}

bool QnBusinessRuleViewModel::isValid() const {
    return m_disabled || (isValid(QnBusiness::SourceColumn) && isValid(QnBusiness::TargetColumn));
}

bool QnBusinessRuleViewModel::isValid(int column) const {
    switch (column) {
    case QnBusiness::SourceColumn:
    {
        switch (m_eventType) {
        case QnBusiness::CameraMotionEvent:
            return isResourcesListValid<QnCameraMotionPolicy>(filterEventResources(m_eventResources, m_eventType));
        case QnBusiness::CameraInputEvent:
            return isResourcesListValid<QnCameraInputPolicy>(filterEventResources(m_eventResources, m_eventType));
        default:
            return true;
        }
    }
    case QnBusiness::TargetColumn:
    {
        switch (m_actionType) {
        case QnBusiness::SendMailAction:
        {
            bool any = false;
            for (const QnUserResourcePtr &user: QnBusiness::filteredResources<QnUserResource>(m_actionResources)) {
                QString email = user->getEmail();
                if (email.isEmpty() || !QnEmailAddress::isValid(email))
                    return false;
                any = true;
            }

            QStringList additional = m_actionParams.emailAddress.split(QLatin1Char(';'), QString::SkipEmptyParts);
            foreach(const QString &email, additional) {
                if (email.trimmed().isEmpty())
                    continue;
                if (!QnEmailAddress::isValid(email))
                    return false;
                any = true;
            }
            return any;
        }
        case QnBusiness::CameraRecordingAction:
            return isResourcesListValid<QnCameraRecordingPolicy>(QnBusiness::filteredResources<QnCameraRecordingPolicy::resource_type>(m_actionResources));
        case QnBusiness::CameraOutputAction:
        case QnBusiness::CameraOutputOnceAction:
            return isResourcesListValid<QnCameraOutputPolicy>(QnBusiness::filteredResources<QnCameraOutputPolicy::resource_type>(m_actionResources));
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
            return !m_actionParams.soundUrl.isEmpty();
        case QnBusiness::SayTextAction:
            return !m_actionParams.sayText.isEmpty();
        default:
            break;
        }

        //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
        QnResourceList resources = filterActionResources(m_actionResources, m_actionType);
        if (QnBusiness::requiresCameraResource(m_actionType) && resources.isEmpty()) {
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
    foreach (QnBusiness::EventState val, QnBusiness::allowedEventStates(m_eventType)) {
        QStandardItem *item = new QStandardItem(toggleStateToModelString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
};

void QnBusinessRuleViewModel::updateActionTypesModel() {
    QModelIndexList prolongedActions = m_actionTypesModel->match(m_actionTypesModel->index(0,0),
                                                                 ProlongedActionRole, true, -1, Qt::MatchExactly);

    // what type of actions to show: prolonged or instant
    bool enableProlongedActions = QnBusiness::hasToggleState(m_eventType);
    foreach (QModelIndex idx, prolongedActions) {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }
}

QString QnBusinessRuleViewModel::getSourceText(const bool detailed) const {
    QnResourceList resources = filterEventResources(m_eventResources, m_eventType);
    if (m_eventType == QnBusiness::CameraMotionEvent)
        return QnCameraMotionPolicy::getText(resources, detailed);
    else if (m_eventType == QnBusiness::CameraInputEvent)
        return QnCameraInputPolicy::getText(resources, detailed);
    
    if (!QnBusiness::isResourceRequired(m_eventType)) {
        return tr("<System>");
    } else if (resources.size() == 1) {
        return getResourceName(resources.first());
    } else if (QnBusiness::requiresServerResource(m_eventType)){
        if (resources.isEmpty())
            return tr("<Any Server>");
        else
            return tr("%n Server(s)", "", resources.size());
    } else /*if (QnBusiness::requiresCameraResource(eventType))*/ {
        QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
        if (cameras.isEmpty())
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("<Any Device>"),
                tr("<Any Camera>")
            );
        else
            return QnDeviceDependentStrings::getNumericName(cameras);
    }
}

QString QnBusinessRuleViewModel::getTargetText(const bool detailed) const {
    QnResourceList resources = filterActionResources(m_actionResources, m_actionType);
    switch(m_actionType) {
    case QnBusiness::SendMailAction:
    {
        QStringList additional;
        foreach (QString address, m_actionParams.emailAddress.split(QLatin1Char(';'), QString::SkipEmptyParts)) {
            QString trimmed = address.trimmed();
            if (trimmed.isEmpty())
                continue;
            additional << trimmed;
        }
        return QnUserEmailPolicy::getText(resources,
                                          detailed,
                                          additional);
    }
    case QnBusiness::ShowPopupAction:
    {
        if (m_actionParams.userGroup == QnBusiness::AdminOnly)
            return tr("Administrators Only");
        else
            return tr("All Users");
    }
    case QnBusiness::CameraRecordingAction:
    {
        return QnCameraRecordingPolicy::getText(resources, detailed);
    }
    case QnBusiness::CameraOutputAction:
    case QnBusiness::CameraOutputOnceAction:
    {
        return QnCameraOutputPolicy::getText(resources, detailed);
    }
    case QnBusiness::PlaySoundAction:
    case QnBusiness::PlaySoundOnceAction:
    {
        QString filename = m_actionParams.soundUrl;
        if (filename.isEmpty())
            return tr("Select Sound");
        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
        return soundModel->titleByFilename(filename);
    }
    case QnBusiness::SayTextAction:
    {
        QString text = m_actionParams.sayText;
        if (text.isEmpty())
            return tr("Enter Text");
        return text;
    }
    default:
        break;
    }

    //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
    if (!QnBusiness::requiresCameraResource(m_actionType)) 
        return tr("<System>");

    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.size() == 1) 
        return getResourceName(cameras.first());
    
    if (cameras.isEmpty())
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Select at least one device"),
            tr("Select at least one camera")
        );
    
    return QnDeviceDependentStrings::getNumericName(cameras);  
}

QString QnBusinessRuleViewModel::getAggregationText() const {
    const int MINUTE = 60;
    const int HOUR = MINUTE*60;
    const int DAY = HOUR * 24;

    if (QnBusiness::hasToggleState(m_actionType))
        return tr("Not Applied");

    if (m_aggregationPeriod <= 0)
        return tr("Instant");

    if (m_aggregationPeriod >= DAY && m_aggregationPeriod % DAY == 0)
        return tr("Every %n days", "", m_aggregationPeriod / DAY);

    if (m_aggregationPeriod >= HOUR && m_aggregationPeriod % HOUR == 0)
        return tr("Every %n hours", "", m_aggregationPeriod / HOUR);

    if (m_aggregationPeriod >= MINUTE && m_aggregationPeriod % MINUTE == 0)
        return tr("Every %n minutes", "", m_aggregationPeriod / MINUTE);

    return tr("Every %n seconds", "", m_aggregationPeriod);
}

QString QnBusinessRuleViewModel::toggleStateToModelString(QnBusiness::EventState value) {
    switch( value )
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
