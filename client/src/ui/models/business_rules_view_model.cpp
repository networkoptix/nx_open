#include "business_rules_view_model.h"

#include <events/abstract_business_event.h>
#include <events/abstract_business_action.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

#include <utils/settings.h>

namespace {

    enum ColumnCount {
        ModifiedColumn,
        EventColumn,
        SourceColumn,
        SpacerColumn,
        ActionColumn,
        TargetColumn,
        ColumnCount
    };

    static QLatin1String prolongedEvent("While %1");
    static QLatin1String instantEvent("On %1 %2");

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

}

QnBusinessRuleViewModel::QnBusinessRuleViewModel(QObject *parent):
    base_type(parent),
    m_id(0),
    m_modified(false),
    m_eventType(BusinessEventType::BE_Camera_Motion),
    m_eventState(ToggleState::NotDefined),
    m_actionType(BusinessActionType::BA_ShowPopup),
    m_aggregationPeriod(60)
{

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

    emit dataChanged(this, 0, ColumnCount - 1);
}


QVariant QnBusinessRuleViewModel::getText(const int column) const {
    switch (column) {
        case ModifiedColumn:
            {
                return (m_modified ? QLatin1String("*") : QString());
            }
        case EventColumn:
            {
                return eventTypeString(m_eventType,
                                       m_eventState,
                                       m_actionType);
            }
        case SourceColumn:
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
        case SpacerColumn:
            return tr("->");
        case ActionColumn:
            return BusinessActionType::toString(m_actionType);
        case TargetColumn:
            return QString();
        default:
            break;
    }
    return QVariant();
}

QVariant QnBusinessRuleViewModel::getIcon(const int column) const {
    switch (column) {
        case SourceColumn:
            {
                QnResourceList resources = m_eventResources; //TODO: filtered by type
                if (!BusinessEventType::isResourceRequired(m_eventType)) {
                    return qnResIconCache->icon(QnResourceIconCache::Servers);
                } else if (resources.size() == 1) {
                    QnResourcePtr resource = resources.first();
                    return qnResIconCache->icon(resource->flags(), resource->getStatus());
                } else if (BusinessEventType::requiresServerResource(m_eventType)){
                    return qnResIconCache->icon(QnResourceIconCache::Server);
                } else /*if (BusinessEventType::requiresCameraResource(m_eventType))*/ {
                    return qnResIconCache->icon(QnResourceIconCache::Camera);
                }
            }
        case TargetColumn:
            {
                break;
            }
        default:
            break;
    }
    return QVariant();
}


////////////////////////////////////////////////////////////////////////////////
////// ----------------- QnBusinessRulesViewModel ----------------------////////
////////////////////////////////////////////////////////////////////////////////

QnBusinessRulesViewModel::QnBusinessRulesViewModel(QObject *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent)
{
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

    return ColumnCount;
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
        case ModifiedColumn:    return tr("#");
        case EventColumn:       return tr("Event");
        case SourceColumn:      return tr("Source");
        case SpacerColumn:      return tr("->");
        case ActionColumn:      return tr("Action");
        case TargetColumn:      return tr("Target");
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
        m_rules << ruleModel;
    }
}
