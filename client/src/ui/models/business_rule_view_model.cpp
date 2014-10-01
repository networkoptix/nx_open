#include "business_rule_view_model.h"

#include <core/resource/resource.h>
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
#include <utils/common/email.h>
#include <utils/media/audio_player.h>

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;
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

    for (int i = 1; i < QnBusiness::EventCount; i++) {
        QnBusiness::EventType val = (QnBusiness::EventType)i;

        QStandardItem *item = new QStandardItem(QnBusinessStringsHelper::eventName(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }

    QList<QnBusiness::EventState> values;
    values << QnBusiness::ActiveState << QnBusiness::InactiveState;
    foreach (QnBusiness::EventState val, values) {
        QStandardItem *item = new QStandardItem(toggleStateToModelString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }

    for (int i = 1; i < QnBusiness::ActionCount; i++) {
        QnBusiness::ActionType val = (QnBusiness::ActionType)i;
        if (!QnBusiness::isImplemented(val))
            continue;

        QStandardItem *item = new QStandardItem(QnBusinessStringsHelper::actionName(val));
        item->setData(val);
        item->setData(QnBusiness::hasToggleState(val), ProlongedActionRole);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }
    updateActionTypesModel();
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
                return m_actionParams.getEmailAddress();
            case QnBusiness::ShowPopupAction:
                return (int)m_actionParams.getUserGroup();
            case QnBusiness::PlaySoundAction:
            case QnBusiness::PlaySoundOnceAction:
                return m_actionParams.getSoundUrl();
            case QnBusiness::SayTextAction:
                return m_actionParams.getSayText();
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
        if (m_system)
            return QBrush(Qt::yellow);
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
        return !QnBusiness::hasToggleState(m_eventType);
    case Qn::ShortTextRole:
        return getText(column, false);

    case Qn::EventTypeRole:
        return m_eventType;
    case Qn::EventResourcesRole:
        return QVariant::fromValue<QnResourceList>(m_eventResources);
    case Qn::ActionTypeRole:
        return m_actionType;
    case Qn::ActionResourcesRole:
        return QVariant::fromValue<QnResourceList>(m_actionResources);

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
        setEventResources(value.value<QnResourceList>());
        return true;
    case QnBusiness::TargetColumn:
        switch(m_actionType) {
        case QnBusiness::ShowPopupAction:
        {
            QnBusinessActionParameters params = m_actionParams;

            // TODO: #GDM #Business you're implicitly relying on what enum values are, which is very bad.
            // This code will fail silently if someone changes the header. Please write it properly.
            
            params.setUserGroup((QnBusinessActionParameters::UserGroup)value.toInt()); 
            setActionParams(params);
            break;
        }
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
        {
            QnBusinessActionParameters params;
            params.setSoundUrl(value.toString());
            setActionParams(params);
            break;
        }
        case QnBusiness::SayTextAction:
        {
            QnBusinessActionParameters params;
            params.setSayText(value.toString());
            setActionParams(params);
            break;
        }
        default:
            setActionResources(value.value<QnResourceList>());
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
    m_eventType = businessRule->eventType();

    m_eventResources.clear();
    m_eventResources.append(businessRule->eventResourceObjects());

    m_eventParams = businessRule->eventParams();

    m_eventState = businessRule->eventState();

    m_actionType = businessRule->actionType();

    m_actionResources.clear();
    m_actionResources.append(businessRule->actionResourceObjects());

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
    if (QnBusiness::requiresCameraResource(m_eventType))
        rule->setEventResources(toIdList(m_eventResources.filtered<QnVirtualCameraResource>()));
    else if (QnBusiness::requiresServerResource(m_eventType))
        rule->setEventResources(toIdList(m_eventResources.filtered<QnMediaServerResource>()));
    else
        rule->setEventResources(QVector<QnUuid>());
    rule->setEventState(m_eventState);   //TODO: #GDM #Business check
    rule->setEventParams(m_eventParams); //TODO: #GDM #Business filtered
    rule->setActionType(m_actionType);
    if (QnBusiness::requiresCameraResource(m_actionType))
        rule->setActionResources(toIdList(m_actionResources.filtered<QnVirtualCameraResource>()));
    else if (QnBusiness::requiresUserResource(m_actionType))
        rule->setActionResources(toIdList(m_actionResources.filtered<QnUserResource>()));
    else
        rule->setActionResources(QVector<QnUuid>());
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
        m_eventState = QnBusiness::ActiveState;
        fields |= QnBusiness::EventStateField;
    }
    if (!QnBusiness::hasToggleState(m_eventType) && QnBusiness::hasToggleState(m_actionType)) {
        m_actionType = QnBusiness::ShowPopupAction;
        fields |= QnBusiness::ActionTypeField | QnBusiness::ActionResourcesField | QnBusiness::ActionParamsField;
    }

    updateActionTypesModel();

    emit dataChanged(this, fields);
    //TODO: #GDM #Business check others, params and resources should be merged
}


QnResourceList QnBusinessRuleViewModel::eventResources() const {
    return m_eventResources;
}

void QnBusinessRuleViewModel::setEventResources(const QnResourceList &value) {
    if (m_eventResources == value)
        return; //TODO: #GDM #Business check equal

    m_eventResources = value;
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

    if (QnBusiness::hasToggleState(m_eventType) &&
            !QnBusiness::hasToggleState(m_actionType) &&
            m_eventState == QnBusiness::UndefinedState) {
        m_eventState = QnBusiness::ActiveState;
        fields |= QnBusiness::EventStateField;
    }

    emit dataChanged(this, fields);
}

QnResourceList QnBusinessRuleViewModel::actionResources() const {
    return m_actionResources;
}

void QnBusinessRuleViewModel::setActionResources(const QnResourceList &value) {
    if (m_actionResources == value)
        return;

    m_actionResources = value;
    m_modified = true;

    emit dataChanged(this, QnBusiness::ActionResourcesField | QnBusiness::ModifiedField);
}

QnBusinessActionParameters QnBusinessRuleViewModel::actionParams() const
{
    return m_actionParams;
}

void QnBusinessRuleViewModel::setActionParams(const QnBusinessActionParameters &params)
{
    bool hasChanges = !params.equalTo(m_actionParams);
    if (!hasChanges)
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
        return eventTypeString(m_eventType,
                               m_eventState,
                               m_actionType);
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
        QnResourceList resources = m_eventResources; //TODO: #GDM #Business filtered by type
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
            if (m_actionParams.getUserGroup() == QnBusinessActionParameters::AdminOnly)
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
        QnResourceList resources = m_actionResources; //TODO: #GDM #Business filtered by type
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
        //TODO: #GDM #Business special icon for sound action
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
            return isResourcesListValid<QnCameraMotionPolicy>(m_eventResources);
        case QnBusiness::CameraInputEvent:
            return isResourcesListValid<QnCameraInputPolicy>(m_eventResources);
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
            foreach (const QnUserResourcePtr &user, m_actionResources.filtered<QnUserResource>()) {
                QString email = user->getEmail();
                if (email.isEmpty() || !QnEmail::isValid(email))
                    return false;
                any = true;
            }

            QStringList additional = m_actionParams.getEmailAddress().split(QLatin1Char(';'), QString::SkipEmptyParts);
            foreach(const QString &email, additional) {
                if (email.trimmed().isEmpty())
                    continue;
                if (!QnEmail::isValid(email))
                    return false;
                any = true;
            }
            return any;
        }
        case QnBusiness::CameraRecordingAction:
            return isResourcesListValid<QnCameraRecordingPolicy>(m_actionResources);
        case QnBusiness::CameraOutputAction:
        case QnBusiness::CameraOutputOnceAction:
            return isResourcesListValid<QnCameraOutputPolicy>(m_actionResources);
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
            return !m_actionParams.getSoundUrl().isEmpty();
        case QnBusiness::SayTextAction:
            return !m_actionParams.getSayText().isEmpty();
        default:
            break;
        }

        //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
        QnResourceList resources = m_actionResources.filtered<QnVirtualCameraResource>();
        if (QnBusiness::requiresCameraResource(m_actionType) && resources.isEmpty()) {
            return false;
        }
    }
    default:
        break;
    }
    return true;
}

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
    if (m_eventType == QnBusiness::CameraMotionEvent)
        return QnCameraMotionPolicy::getText(m_eventResources, detailed);
    else if (m_eventType == QnBusiness::CameraInputEvent)
        return QnCameraInputPolicy::getText(m_eventResources, detailed);

    QnResourceList resources = m_eventResources; //TODO: #GDM #Business filtered by type
    if (!QnBusiness::isResourceRequired(m_eventType)) {
        return tr("<System>");
    } else if (resources.size() == 1) {
        return getResourceName(resources.first());
    } else if (QnBusiness::requiresServerResource(m_eventType)){
        if (resources.size() == 0)
            return tr("<Any Server>");
        else
            return tr("%n Server(s)", "", resources.size());
    } else /*if (QnBusiness::requiresCameraResource(eventType))*/ {
        if (resources.size() == 0)
            return tr("<Any Camera>");
        else
            return tr("%n Camera(s)", "", resources.size());
    }
}

QString QnBusinessRuleViewModel::getTargetText(const bool detailed) const {
    switch(m_actionType) {
    case QnBusiness::SendMailAction:
    {
        QStringList additional;
        foreach (QString address, m_actionParams.getEmailAddress().split(QLatin1Char(';'), QString::SkipEmptyParts)) {
            QString trimmed = address.trimmed();
            if (trimmed.isEmpty())
                continue;
            additional << trimmed;
        }
        return QnUserEmailPolicy::getText(m_actionResources,
                                          detailed,
                                          additional);
    }
    case QnBusiness::ShowPopupAction:
    {
        if (m_actionParams.getUserGroup() == QnBusinessActionParameters::AdminOnly)
            return tr("Administrators only");
        else
            return tr("All users");
    }
    case QnBusiness::CameraRecordingAction:
    {
        return QnCameraRecordingPolicy::getText(m_actionResources, detailed);
    }
    case QnBusiness::CameraOutputAction:
    case QnBusiness::CameraOutputOnceAction:
    {
        return QnCameraOutputPolicy::getText(m_actionResources, detailed);
    }
    case QnBusiness::PlaySoundAction:
    case QnBusiness::PlaySoundOnceAction:
    {
        QString filename = m_actionParams.getSoundUrl();
        if (filename.isEmpty())
            return tr("Select a sound");
        QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
        return soundModel->titleByFilename(filename);
    }
    case QnBusiness::SayTextAction:
    {
        QString text = m_actionParams.getSayText();
        if (text.isEmpty())
            return tr("Enter text");
        return text;
    }
    default:
        break;
    }

    //TODO: #GDM #Business check all variants or resource requirements: userResource, serverResource
    QnResourceList resources = m_actionResources;
    if (!QnBusiness::requiresCameraResource(m_actionType)) {
        return tr("<System>");
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        return getResourceName(resource);
    } else if (resources.isEmpty()) {
        return tr("Select at least one camera");
    } else {
        return tr("%n Camera(s)", "", resources.size());
    }

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
        return tr("Starts/Stops");
    }
    return QString();
}

QString QnBusinessRuleViewModel::toggleStateToString(QnBusiness::EventState state) {
    switch (state) {
    case QnBusiness::ActiveState:
        return tr("start");
    case QnBusiness::InactiveState:
        return tr("stop");
    default:
        break;
    }
    return QString();
}

QString QnBusinessRuleViewModel::eventTypeString(QnBusiness::EventType eventType, QnBusiness::EventState eventState, QnBusiness::ActionType actionType) {
    QString typeStr = QnBusinessStringsHelper::eventName(eventType);
    if (QnBusiness::hasToggleState(actionType))
        return tr("While %1").arg(typeStr);
    else
        return tr("On %1 %2").arg(typeStr).arg(toggleStateToString(eventState));
}
