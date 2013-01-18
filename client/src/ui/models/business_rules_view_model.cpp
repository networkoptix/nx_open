#include "business_rules_view_model.h"

#include <QRegExp>

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

    QString toggleStateToString(ToggleState::Value value, bool prolonged) {
        switch( value )
        {
            case ToggleState::Off:
                return QObject::tr("Stops");
            case ToggleState::On:
                return QObject::tr("Starts");
            case ToggleState::NotDefined:
            if (prolonged)
                return QObject::tr("Starts/Stops");
            else
                return QObject::tr("Occurs");
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

}

////////////////////////////////////////////////////////////////////////////////
////// ----------------- QnBusinessRuleViewModel -----------------------////////
////////////////////////////////////////////////////////////////////////////////

QnBusinessRuleViewModel::QnBusinessRuleViewModel(QObject *parent):
    base_type(parent),
    m_id(0),
    m_modified(false),
    m_eventType(BusinessEventType::BE_Camera_Motion),
    m_eventState(ToggleState::NotDefined),
    m_actionType(BusinessActionType::BA_ShowPopup),
    m_aggregationPeriod(60),
    m_eventTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this))
{
    updateEventTypesModel();
}

QVariant QnBusinessRuleViewModel::data(const int column, const int role) const {
    switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            return getText(column);
        case Qt::DecorationRole:
            return getIcon(column);
        default:
            break;
    }
    return QVariant();
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

    updateEventStatesModel(); //TODO: connect on dataChanged?
    updateActionTypesModel();

    emit dataChanged(this, QnBusiness::AllFieldsMask);
}

bool QnBusinessRuleViewModel::actionTypeShouldBeInstant() {
    return (m_eventState == ToggleState::On || m_eventState == ToggleState::Off)
                || (!BusinessEventType::hasToggleState(m_eventType));
}

// setters and getters

BusinessEventType::Value QnBusinessRuleViewModel::eventType() const {
    return m_eventType;
}

void QnBusinessRuleViewModel::setEventType(const BusinessEventType::Value value) {
    if (m_eventType == value)
        return;

    m_eventType = value;
    m_modified = true;

    updateEventStatesModel();
    updateActionTypesModel();

    emit dataChanged(this, QnBusiness::EventTypeField | QnBusiness::ModifiedField);

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
    updateActionTypesModel();

    emit dataChanged(this, QnBusiness::EventStateField | QnBusiness::ModifiedField);
}

BusinessActionType::Value QnBusinessRuleViewModel::actionType() const {
    return m_actionType;
}

void QnBusinessRuleViewModel::setActionType(const BusinessActionType::Value value) {
    if (m_actionType == value)
        return;

    m_actionType = value;
    m_modified = true;

    emit dataChanged(this, QnBusiness::ActionTypeField | QnBusiness::ModifiedField);
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
            {
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
                        return tr("%1 Servers").arg(resources.size()); //TODO: fix tr to %n
                } else /*if (BusinessEventType::requiresCameraResource(eventType))*/ {
                    if (resources.size() == 0)
                        return tr("<Any Camera>");
                    else
                        return tr("%1 Cameras").arg(resources.size()); //TODO: fix tr to %n
                }
            }
        case QnBusiness::SpacerColumn:
            return tr("->");
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

void QnBusinessRuleViewModel::updateEventTypesModel() {
    m_eventTypesModel->clear();
    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        BusinessEventType::Value val = (BusinessEventType::Value)i;

        QStandardItem *item = new QStandardItem(BusinessEventType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventTypesModel->appendRow(row);
    }
}

void QnBusinessRuleViewModel::updateEventStatesModel() {
    m_eventStatesModel->clear();

    QList<ToggleState::Value> values;
    values << ToggleState::NotDefined;
    bool prolonged = BusinessEventType::hasToggleState(m_eventType);
    if (prolonged)
        values << ToggleState::On << ToggleState::Off;

    foreach (ToggleState::Value val, values) {
        QStandardItem *item = new QStandardItem(toggleStateToString(val, prolonged));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
}

void QnBusinessRuleViewModel::updateActionTypesModel() {
    m_actionTypesModel->clear();
    // what type of actions to show: prolonged or instant
    bool onlyInstantActions = actionTypeShouldBeInstant();

    for (int i = 0; i < BusinessActionType::BA_Count; i++) {
        BusinessActionType::Value val = (BusinessActionType::Value)i;

        if (BusinessActionType::hasToggleState(val) && onlyInstantActions)
            continue;

        QStandardItem *item = new QStandardItem(BusinessActionType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
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
            return tr("Show popup for administrators");
        else
            return tr("Show popup for all users");
    }

    QnResourceList resources = m_actionResources; //TODO: filtered by type
    if (!BusinessActionType::isResourceRequired(m_actionType)) {
        return tr("<System>");
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        return getResourceName(resource);
    } else if (resources.isEmpty()) {
        return tr("Select at least one camera");
    } else {
        return tr("%1 Cameras").arg(resources.size()); //TODO: fix tr to %n
    }

}


////////////////////////////////////////////////////////////////////////////////
////// ----------------- QnBusinessRulesViewModel ----------------------////////
////////////////////////////////////////////////////////////////////////////////

QnBusinessRulesViewModel::QnBusinessRulesViewModel(QObject *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent)
{
    m_fieldsByColumn[QnBusiness::ModifiedColumn] = QnBusiness::ModifiedField;
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

void QnBusinessRulesViewModel::clear() {
    m_rules.clear();
    reset();
}

void QnBusinessRulesViewModel::addRules(const QnBusinessEventRules &businessRules) {
    foreach (QnBusinessEventRulePtr rule, businessRules) {
        QnBusinessRuleViewModel* ruleModel = new QnBusinessRuleViewModel(this);
        ruleModel->loadFromRule(rule);
        connect(ruleModel, SIGNAL(dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)),
                this, SLOT(at_rule_dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)));
        m_rules << ruleModel;
    }
}

QnBusinessRuleViewModel* QnBusinessRulesViewModel::getRuleModel(int row) {
    if (row >= m_rules.size())
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
