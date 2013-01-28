#include "business_rules_view_model.h"

#include <QRegExp>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <events/abstract_business_event.h>
#include <events/abstract_business_action.h>
#include <events/sendmail_business_action.h>
#include <events/popup_business_action.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

#include <utils/settings.h>

namespace {

    static QLatin1String prolongedEvent("While %1");
    static QLatin1String instantEvent("On %1 %2");

    QString toggleStateToModelString(ToggleState::Value value) {
        switch( value )
        {
            case ToggleState::Off:
                return QObject::tr("Stops");
            case ToggleState::On:
                return QObject::tr("Starts");
            case ToggleState::NotDefined:
                return QObject::tr("Starts/Stops");
        }
        return QString();
    }

    QString toggleStateToString(ToggleState::Value state) {
        switch (state) {
        case ToggleState::On: return QObject::tr("start");
        case ToggleState::Off: return QObject::tr("stop");
            default: return QString();
        }
        return QString();
    }

    QString eventTypeString(BusinessEventType::Value eventType,
                            ToggleState::Value eventState,
                            BusinessActionType::Value actionType){
        QString typeStr = BusinessEventType::toString(eventType);
        if (BusinessActionType::hasToggleState(actionType))
            return QString(prolongedEvent).arg(typeStr);
        else
            return QString(instantEvent).arg(typeStr)
                    .arg(toggleStateToString(eventState));
    }

    QString extractHost(const QString &url) {
        /* Try it as a host address first. */
        QHostAddress hostAddress(url);
        if(!hostAddress.isNull())
            return hostAddress.toString();

        /* Then go default QUrl route. */
        return QUrl(url).host();
    }

    QString getResourceName(const QnResourcePtr& resource) {
        if (!resource)
            return QObject::tr("<select target>");

        QnResource::Flags flags = resource->flags();
        if (qnSettings->isIpShownInTree()) {
            if((flags & QnResource::network) || (flags & QnResource::server && flags & QnResource::remote)) {
                QString host = extractHost(resource->getUrl());
                if(!host.isEmpty())
                    return QString(QLatin1String("%1 (%2)")).arg(resource->getName()).arg(host);
            }
        }
        return resource->getName();
    }

    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    bool isEmailValid(QString email) {
        QRegExp rx(emailPattern);
        return rx.exactMatch(email.toUpper());
    }

    bool isEmailValid(QStringList emails) {
        foreach (QString email, emails)
            if (!isEmailValid(email))
                return false;
        return true;
    }

    const int ProlongedActionRole = Qt::UserRole + 2;

}

////////////////////////////////////////////////////////////////////////////////
////// ----------------- QnBusinessRuleViewModel -----------------------////////
////////////////////////////////////////////////////////////////////////////////

QnBusinessRuleViewModel::QnBusinessRuleViewModel(QObject *parent):
    base_type(parent),
    m_id(0),
    m_modified(false),
    m_eventType(BusinessEventType::BE_Camera_Disconnect),
    m_eventState(ToggleState::NotDefined),
    m_actionType(BusinessActionType::BA_ShowPopup),
    m_aggregationPeriod(60),
    m_disabled(false),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this))
{
    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        BusinessEventType::Value val = (BusinessEventType::Value)i;

        QStandardItem *item = new QStandardItem(BusinessEventType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }

    QList<ToggleState::Value> values;
    values << ToggleState::NotDefined << ToggleState::On << ToggleState::Off;
    foreach (ToggleState::Value val, values) {
        QStandardItem *item = new QStandardItem(toggleStateToModelString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }

    for (int i = 0; i < BusinessActionType::BA_Count; i++) {
        BusinessActionType::Value val = (BusinessActionType::Value)i;

        QStandardItem *item = new QStandardItem(BusinessActionType::toString(val));
        item->setData(val);
        item->setData(BusinessActionType::hasToggleState(val), ProlongedActionRole);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }
    updateActionTypesModel();
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
                return m_comments.isEmpty() ? QVariant() : m_comments;

            default:
                break;
        }

//        return QVariant();
    }

    switch (role) {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return getText(column);

        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleDescriptionRole:
            return m_comments.isEmpty() ? getText(column) : m_comments;

        case Qt::DecorationRole:
            return getIcon(column);

        case Qt::EditRole:
            if (column == QnBusiness::EventColumn)
                return m_eventType;
            else if (column == QnBusiness::ActionColumn)
                return m_actionType;
            else if (column == QnBusiness::TargetColumn) {
                if (m_actionType == BusinessActionType::BA_SendMail)
                    return BusinessActionParameters::getEmailAddress(m_actionParams);
                if (m_actionType == BusinessActionType::BA_ShowPopup)
                    return BusinessActionParameters::getUserGroup(m_actionParams);
            }
            break;

        case Qt::TextColorRole:
//            if (m_disabled || isValid())
                break;

//            if (!isValid(column))
//                return QBrush(Qt::black);
//            return QBrush(Qt::black); //test

        case Qt::BackgroundRole:
            if (m_disabled || isValid())
                break;

            if (!isValid(column))
                return QBrush(QColor(204, 0, 0));   //TODO: #GDM skin colors
            return QBrush(QColor(150, 0, 0));       //TODO: #GDM skin colors

        case QnBusiness::ModifiedRole:
            return m_modified;
        case QnBusiness::DisabledRole:
            return m_disabled;
        case QnBusiness::ValidRole:
            return isValid();
        case QnBusiness::InstantActionRole:
            return actionTypeShouldBeInstant();
        case QnBusiness::ShortTextRole:
            return getText(column, false);

        case QnBusiness::EventTypeRole:
            return m_eventType;
        case QnBusiness::EventResourcesRole:
            return QVariant::fromValue<QnResourceList>(m_eventResources);
        case QnBusiness::ActionTypeRole:
            return m_actionType;
        case QnBusiness::ActionResourcesRole:
            return QVariant::fromValue<QnResourceList>(m_actionResources);
        default:
            break;
    }
    return QVariant();
}

bool QnBusinessRuleViewModel::setData(const int column, const QVariant &value, int role) {
    if (column == QnBusiness::DisabledColumn && role == Qt::CheckStateRole) {
        Qt::CheckState checked = (Qt::CheckState)value.toInt();
        setDisabled(checked == Qt::Unchecked);
        return true;
    }

    if (role != Qt::EditRole)
        return false;

    switch (column) {
        case QnBusiness::EventColumn:
            setEventType((BusinessEventType::Value)value.toInt());
            return true;
        case QnBusiness::ActionColumn:
            setActionType((BusinessActionType::Value)value.toInt());
            return true;
        case QnBusiness::SourceColumn:
            setEventResources(value.value<QnResourceList>());
            return true;
        case QnBusiness::TargetColumn:
            if (m_actionType == BusinessActionType::BA_SendMail) {
                QnBusinessParams params = m_actionParams;
                BusinessActionParameters::setEmailAddress(&params, value.toString());
                setActionParams(params);
            } else if (m_actionType == BusinessActionType::BA_ShowPopup) {
                QnBusinessParams params = m_actionParams;
                BusinessActionParameters::setUserGroup(&params, value.toInt());
                setActionParams(params);
            }
            else
                setActionResources(value.value<QnResourceList>());
            return true;
        default:
            break;
    }

    return false;
}


void QnBusinessRuleViewModel::loadFromRule(QnBusinessEventRulePtr businessRule) {
    m_id = businessRule->getId();
    m_modified = false;
    m_eventType = businessRule->eventType();

    m_eventResources.clear();
    m_eventResources.append(businessRule->eventResources());

    foreach (QString key, businessRule->eventParams().keys())
        m_eventParams[key] = businessRule->eventParams()[key];

    m_eventState = businessRule->eventState();

    m_actionType = businessRule->actionType();

    m_actionResources.clear();
    m_actionResources.append(businessRule->actionResources());

    foreach (QString key, businessRule->actionParams().keys())
        m_actionParams[key] = businessRule->actionParams()[key];

    m_aggregationPeriod = businessRule->aggregationPeriod();

    m_disabled = businessRule->isDisabled();
    m_comments = businessRule->comments();
    m_schedule = businessRule->schedule();

    updateActionTypesModel();//TODO: connect on dataChanged?

    emit dataChanged(this, QnBusiness::AllFieldsMask);
}

bool QnBusinessRuleViewModel::actionTypeShouldBeInstant() const {
    return (m_eventState != ToggleState::NotDefined)
                || (!BusinessEventType::hasToggleState(m_eventType));
}

QnBusinessEventRulePtr QnBusinessRuleViewModel::createRule() const {
    QnBusinessEventRulePtr rule(new QnBusinessEventRule());
    rule->setId(m_id);
    rule->setEventType(m_eventType);
    rule->setEventResources(m_eventResources); //TODO: #GDM filtered
    rule->setEventState(m_eventState);
    rule->setEventParams(m_eventParams);
    rule->setActionType(m_actionType);
    rule->setActionResources(m_actionResources);
    rule->setActionParams(m_actionParams);
    rule->setAggregationPeriod(m_aggregationPeriod);
    rule->setDisabled(m_disabled);
    rule->setComments(m_comments);
    rule->setSchedule(m_schedule);
    return rule;
}

// setters and getters


QnId QnBusinessRuleViewModel::id() const {
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

BusinessEventType::Value QnBusinessRuleViewModel::eventType() const {
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const BusinessEventType::Value value) {
    if (m_eventType == value)
        return;

    bool cameraRequired = BusinessEventType::requiresCameraResource(m_eventType);
    bool serverRequired = BusinessEventType::requiresServerResource(m_eventType);

    m_eventType = value;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::EventTypeField | QnBusiness::ModifiedField;

    if (!BusinessEventType::hasToggleState(m_eventType) && m_eventState != ToggleState::NotDefined){
        m_eventState = ToggleState::NotDefined;
        fields |= QnBusiness::EventStateField;
    }

    if (BusinessEventType::requiresCameraResource(m_eventType) != cameraRequired ||
             BusinessEventType::requiresServerResource(m_eventType) != serverRequired) {
        fields |= QnBusiness::EventResourcesField;
    }

    if (actionTypeShouldBeInstant() && BusinessActionType::hasToggleState(m_actionType)) {
        m_actionType = BusinessActionType::BA_ShowPopup;
        fields |= QnBusiness::ActionTypeField;
    }

    updateActionTypesModel();

    emit dataChanged(this, fields);
    //TODO: #GDM check others, params and resources should be merged
}


QnResourceList QnBusinessRuleViewModel::eventResources() const {
    return m_eventResources;
}

void QnBusinessRuleViewModel::setEventResources(const QnResourceList &value) {
    if (m_eventResources == value)
        return; //TODO: check equal

    m_eventResources = value;
    m_modified = true;

    emit dataChanged(this, QnBusiness::EventResourcesField | QnBusiness::ModifiedField);
}

QnBusinessParams QnBusinessRuleViewModel::eventParams() const {
    return m_eventParams;
}

void QnBusinessRuleViewModel::setEventParams(const QnBusinessParams &params)
{
    //TODO: check equal

    m_eventParams = params;
    m_modified = true;

    emit dataChanged(this, QnBusiness::EventParamsField | QnBusiness::ModifiedField);
}

ToggleState::Value QnBusinessRuleViewModel::eventState() const {
    return m_eventState;
}

void QnBusinessRuleViewModel::setEventState(ToggleState::Value state) {
    if (m_eventState == state)
        return;

    m_eventState = state;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::EventStateField | QnBusiness::ModifiedField;

    if (actionTypeShouldBeInstant() && BusinessActionType::hasToggleState(m_actionType)) {
        m_actionType = BusinessActionType::BA_ShowPopup;
        fields |= QnBusiness::ActionTypeField;
    }
    updateActionTypesModel();

    emit dataChanged(this, fields);
}

BusinessActionType::Value QnBusinessRuleViewModel::actionType() const {
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const BusinessActionType::Value value) {
    if (m_actionType == value)
        return;

    bool resourcesRequired = BusinessActionType::isResourceRequired(m_actionType);

    m_actionType = value;
    m_modified = true;

    QnBusiness::Fields fields = QnBusiness::ActionTypeField | QnBusiness::ModifiedField;
    if (BusinessActionType::isResourceRequired(m_actionType) != resourcesRequired)
        fields |= QnBusiness::ActionResourcesField;

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

QnBusinessParams QnBusinessRuleViewModel::actionParams() const
{
    return m_actionParams;
}

void QnBusinessRuleViewModel::setActionParams(const QnBusinessParams &params)
{
    //TODO: check equal

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

QVariant QnBusinessRuleViewModel::getText(const int column, const bool detailed) const {
    switch (column) {
        case QnBusiness::ModifiedColumn:
            {
                return (m_modified ? QLatin1String("*") : QString());
            }
        case QnBusiness::EventColumn:
            {
                return eventTypeString(m_eventType,
                                       m_eventState,
                                       m_actionType);
            }
        case QnBusiness::SourceColumn:
            return getSourceText(detailed);
        case QnBusiness::SpacerColumn:
            return QString();
        case QnBusiness::ActionColumn:
            return BusinessActionType::toString(m_actionType);
        case QnBusiness::TargetColumn:
            return getTargetText(detailed);
        default:
            break;
    }
    return QVariant();
}

QVariant QnBusinessRuleViewModel::getIcon(const int column) const {
    switch (column) {
        case QnBusiness::SourceColumn:
            {
                QnResourceList resources = m_eventResources; //TODO: filtered by type
                if (!BusinessEventType::isResourceRequired(m_eventType)) {
                    return qnResIconCache->icon(QnResourceIconCache::Servers);
                } else if (resources.size() == 1) {
                    QnResourcePtr resource = resources.first();
                    return qnResIconCache->icon(resource->flags(), resource->getStatus());
                } else if (BusinessEventType::requiresServerResource(m_eventType)){
                    return qnResIconCache->icon(QnResourceIconCache::Server);
                } else /* ::requiresCameraResource(m_eventType) */ {
                    return qnResIconCache->icon(QnResourceIconCache::Camera);
                }
            }
        case QnBusiness::TargetColumn:
            {
                if (m_actionType == BusinessActionType::BA_SendMail) {
                    QString email = BusinessActionParameters::getEmailAddress(m_actionParams);
                    QStringList receivers = email.split(QLatin1Char(';'), QString::SkipEmptyParts);
                    if (receivers.isEmpty() || !isEmailValid(receivers))
                        return qnResIconCache->icon(QnResourceIconCache::Offline, true);
                    if (receivers.size() == 1)
                        return qnResIconCache->icon(QnResourceIconCache::User);
                    else
                        return qnResIconCache->icon(QnResourceIconCache::Users);

                } else if (m_actionType == BusinessActionType::BA_ShowPopup) {
                    if (BusinessActionParameters::getUserGroup(m_actionParams) > 0)
                        return qnResIconCache->icon(QnResourceIconCache::User);
                    else
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                }

                QnResourceList resources = m_actionResources; //TODO: filtered by type
                if (!BusinessActionType::isResourceRequired(m_actionType)) {
                    return qnResIconCache->icon(QnResourceIconCache::Servers);
                } else if (resources.size() == 1) {
                    QnResourcePtr resource = resources.first();
                    return qnResIconCache->icon(resource->flags(), resource->getStatus());
                } else if (resources.isEmpty()) {
                    return qnResIconCache->icon(QnResourceIconCache::Offline, true);
                } else {
                    return qnResIconCache->icon(QnResourceIconCache::Camera);
                }
            }
        default:
            break;
    }
    return QVariant();
}

bool QnBusinessRuleViewModel::isValid() const {
    return m_disabled || (isValid(QnBusiness::SourceColumn) && isValid(QnBusiness::TargetColumn));
}

bool QnBusinessRuleViewModel::isValid(int column) const {
    switch (column) {
        case QnBusiness::SourceColumn:
            {
                if (m_eventType != BusinessEventType::BE_Camera_Motion)
                    return true;

                QnVirtualCameraResourceList cameras = m_eventResources.filtered<QnVirtualCameraResource>();
                if (cameras.isEmpty())
                    return true; // should no check if any camera is selected

                bool valid = true;
                foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
                    if (camera->isScheduleDisabled()) {
                        valid = false;
                        break;
                    }
                }
                return valid;
            }
        case QnBusiness::TargetColumn:
            {
                if (m_actionType == BusinessActionType::BA_SendMail) {
                    QString email = BusinessActionParameters::getEmailAddress(m_actionParams);
                    QStringList receivers = email.split(QLatin1Char(';'), QString::SkipEmptyParts);
                    if (receivers.isEmpty() || !isEmailValid(receivers))
                        return false;
                    return true;
                } else if (m_actionType == BusinessActionType::BA_CameraRecording) {
                    QnVirtualCameraResourceList cameras = m_actionResources.filtered<QnVirtualCameraResource>();
                    bool valid = cameras.size() > 0;
                    if (valid) {
                        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
                            if (camera->isScheduleDisabled()) {
                                valid = false;
                                break;
                            }
                        }
                    }
                    return valid;
                }

                QnResourceList resources = m_actionResources.filtered<QnVirtualCameraResource>(); //TODO: filtered by type
                if (BusinessActionType::isResourceRequired(m_actionType) && resources.isEmpty()) {
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
    bool enableProlongedActions = !actionTypeShouldBeInstant();
    foreach (QModelIndex idx, prolongedActions) {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }
}

QString QnBusinessRuleViewModel::getSourceText(const bool detailed) const {
    if (m_eventType == BusinessEventType::BE_Camera_Motion) {
        QnVirtualCameraResourceList cameras = m_eventResources.filtered<QnVirtualCameraResource>();
        if (cameras.isEmpty())
            return tr("<Any Camera>");

        int disabled = 0;
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            if (camera->isScheduleDisabled()) {
                disabled++;
            }
        }
        if (detailed && disabled > 0)
            return tr("Recording is disabled for %1")
                    .arg((cameras.size() == 1)
                         ? getResourceName(cameras.first())
                         : tr("%1 of %2 cameras").arg(disabled).arg(cameras.size()));
        if (cameras.size() == 1)
            return getResourceName(cameras.first());
        return tr("%n Camera(s)", NULL, cameras.size());
    }

    QnResourceList resources = m_eventResources; //TODO: filtered by type
    if (!BusinessEventType::isResourceRequired(m_eventType)) {
        return tr("<System>");
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        return getResourceName(resource);
    } else if (BusinessEventType::requiresServerResource(m_eventType)){
        if (resources.size() == 0)
            return tr("<Any Server>");
        else
            return tr("%n Server(s)", NULL, resources.size());
    } else /*if (BusinessEventType::requiresCameraResource(eventType))*/ {
        if (resources.size() == 0)
            return tr("<Any Camera>");
        else
            return tr("%n Camera(s)", NULL, resources.size());
    }
}

QString QnBusinessRuleViewModel::getTargetText(const bool detailed) const {
    if (m_actionType == BusinessActionType::BA_SendMail) {
        QString email = BusinessActionParameters::getEmailAddress(m_actionParams);
        QStringList receivers = email.split(QLatin1Char(';'), QString::SkipEmptyParts);
        if (receivers.isEmpty() )
            return tr("Enter at least one email address");
        foreach (QString receiver, receivers) {
            if (!isEmailValid(receiver))
                return tr("Invalid email address: %1").arg(receiver);
        }
        return tr("Send mail to %1").arg(receivers.join(QLatin1String("; ")));

    } else if (m_actionType == BusinessActionType::BA_ShowPopup) {
        if (BusinessActionParameters::getUserGroup(m_actionParams) > 0)
            return tr("Administrators only");
        else
            return tr("All users");
    } else if (m_actionType == BusinessActionType::BA_CameraRecording) {
        QnVirtualCameraResourceList cameras = m_actionResources.filtered<QnVirtualCameraResource>();
        if (cameras.isEmpty())
            return tr("Select at least one camera");

        int disabled = 0;
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            if (camera->isScheduleDisabled()) {
                disabled++;
            }
        }
        if (detailed && disabled > 0)
            return tr("Recording is disabled for %1")
                    .arg((cameras.size() == 1)
                         ? getResourceName(cameras.first())
                         : tr("%1 of %2 cameras").arg(disabled).arg(cameras.size()));
        if (cameras.size() == 1)
            return getResourceName(cameras.first());
        return tr("%n Camera(s)", NULL, cameras.size());
    }

    QnResourceList resources = m_actionResources;
    if (!BusinessActionType::isResourceRequired(m_actionType)) {
        return tr("<System>");
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        return getResourceName(resource);
    } else if (resources.isEmpty()) {
        return tr("Select at least one camera");
    } else {
        return tr("%n Camera(s)", NULL, resources.size());
    }

}


////////////////////////////////////////////////////////////////////////////////
////// ----------------- QnBusinessRulesViewModel ----------------------////////
////////////////////////////////////////////////////////////////////////////////

QnBusinessRulesViewModel::QnBusinessRulesViewModel(QObject *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context)
{
    m_fieldsByColumn[QnBusiness::ModifiedColumn] = QnBusiness::ModifiedField;
    m_fieldsByColumn[QnBusiness::DisabledColumn] = QnBusiness::DisabledField;
    m_fieldsByColumn[QnBusiness::EventColumn] = QnBusiness::EventTypeField | QnBusiness::EventStateField | QnBusiness::ActionTypeField;
    m_fieldsByColumn[QnBusiness::SourceColumn] = QnBusiness::EventTypeField | QnBusiness::EventResourcesField;
    m_fieldsByColumn[QnBusiness::SpacerColumn] = 0;
    m_fieldsByColumn[QnBusiness::ActionColumn] = QnBusiness::ActionTypeField;
    m_fieldsByColumn[QnBusiness::TargetColumn] = QnBusiness::ActionTypeField | QnBusiness::ActionParamsField | QnBusiness::ActionResourcesField;
}

QModelIndex QnBusinessRulesViewModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column, 0) : QModelIndex();
}

QModelIndex QnBusinessRulesViewModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)

    return QModelIndex();
}

int QnBusinessRulesViewModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    return m_rules.size();
}

int QnBusinessRulesViewModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    return QnBusiness::ColumnCount;
}

QVariant QnBusinessRulesViewModel::data(const QModelIndex &index, int role) const {
    return m_rules[index.row()]->data(index.column(), role);
}

bool QnBusinessRulesViewModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    return m_rules[index.row()]->setData(index.column(), value, role);
}

QVariant QnBusinessRulesViewModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return QVariant();

    switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            break;
        default:
            return QVariant();
    }

    switch (section) {
        case QnBusiness::ModifiedColumn:    return tr("#");
        case QnBusiness::DisabledColumn:    return tr("On");
        case QnBusiness::EventColumn:       return tr("Event");
        case QnBusiness::SourceColumn:      return tr("Source");
        case QnBusiness::SpacerColumn:      return tr("->");
        case QnBusiness::ActionColumn:      return tr("Action");
        case QnBusiness::TargetColumn:      return tr("Target");
        default:
            break;
    }

    return QVariant();
}

Qt::ItemFlags QnBusinessRulesViewModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = base_type::flags(index);

    switch (index.column()) {
        case QnBusiness::DisabledColumn:
            flags |= Qt::ItemIsUserCheckable;
            break;
        case QnBusiness::EventColumn:
        case QnBusiness::ActionColumn:
            flags |= Qt::ItemIsEditable;
            break;
        case QnBusiness::SourceColumn:
            if (BusinessEventType::isResourceRequired(m_rules[index.row()]->eventType()))
                flags |= Qt::ItemIsEditable;
            break;
        case QnBusiness::TargetColumn:
            {
                BusinessActionType::Value actionType = m_rules[index.row()]->actionType();
                if (BusinessActionType::isResourceRequired(actionType)
                        || actionType == BusinessActionType::BA_SendMail
                        || actionType == BusinessActionType::BA_ShowPopup)
                    flags |= Qt::ItemIsEditable;
            }
            break;
        default:
            break;
    }
    return flags;
}

void QnBusinessRulesViewModel::clear() {
    m_rules.clear();
    reset();
}

void QnBusinessRulesViewModel::addRules(const QnBusinessEventRules &businessRules) {
    foreach (QnBusinessEventRulePtr rule, businessRules) {
        addRule(rule);
    }
}

void QnBusinessRulesViewModel::addRule(QnBusinessEventRulePtr rule) {
    QnBusinessRuleViewModel* ruleModel = new QnBusinessRuleViewModel(this);
    if (rule)
        ruleModel->loadFromRule(rule);
    else
        ruleModel->setModified(true);
    connect(ruleModel, SIGNAL(dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)),
            this, SLOT(at_rule_dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)));

    int row = m_rules.size();
    beginInsertRows(QModelIndex(), row, row);
    m_rules << ruleModel;
    endInsertRows();

    emit dataChanged(index(row, 0), index(row, QnBusiness::ColumnCount - 1));
}

void QnBusinessRulesViewModel::updateRule(QnBusinessEventRulePtr rule) {
    foreach (QnBusinessRuleViewModel* ruleModel, m_rules) {
        if (ruleModel->id() == rule->getId()) {
            ruleModel->loadFromRule(rule);
            return;
        }
    }
    addRule(rule);
}

void QnBusinessRulesViewModel::deleteRule(QnBusinessRuleViewModel *ruleModel) {
    int row = m_rules.indexOf(ruleModel);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_rules.removeAt(row);
    endRemoveRows();

    //TODO: check if dataChanged is required, check row
    //emit dataChanged(index(row, 0), index(row, QnBusiness::ColumnCount - 1));
}

void QnBusinessRulesViewModel::deleteRule(QnId id) {
    foreach (QnBusinessRuleViewModel* rule, m_rules) {
        if (rule->id() == id) {
            deleteRule(rule);
            return;
        }
    }
}

QnBusinessRuleViewModel* QnBusinessRulesViewModel::getRuleModel(int row) {
    if (row < 0 || row >= m_rules.size())
        return NULL;
    return m_rules[row];
}

void QnBusinessRulesViewModel::at_rule_dataChanged(QnBusinessRuleViewModel *source, QnBusiness::Fields fields) {
    int row = m_rules.indexOf(source);
    if (row < 0)
        return;

    int leftMostColumn = -1;
    int rightMostColumn = QnBusiness::ColumnCount - 1;

    for (int i = 0; i < QnBusiness::ColumnCount; i++) {
        if (fields & m_fieldsByColumn[i]) {
            if (leftMostColumn < 0)
                leftMostColumn = i;
            else
                rightMostColumn = i;
        }
    }

    if (leftMostColumn < 0)
        return;

    QModelIndex index = this->index(row, leftMostColumn, QModelIndex());
    emit dataChanged(index, index.sibling(index.row(), rightMostColumn));
}
