#include "business_rules_view_model.h"

#include <QtCore/QFileInfo>

#include <client/client_settings.h>

#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>


QnBusinessRulesViewModel::QnBusinessRulesViewModel(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    m_fieldsByColumn[QnBusiness::ModifiedColumn] = QnBusiness::ModifiedField;
    m_fieldsByColumn[QnBusiness::DisabledColumn] = QnBusiness::DisabledField;
    m_fieldsByColumn[QnBusiness::EventColumn] = QnBusiness::EventTypeField | QnBusiness::EventStateField | QnBusiness::ActionTypeField;
    m_fieldsByColumn[QnBusiness::SourceColumn] = QnBusiness::EventTypeField | QnBusiness::EventResourcesField;
    m_fieldsByColumn[QnBusiness::SpacerColumn] = 0;
    m_fieldsByColumn[QnBusiness::ActionColumn] = QnBusiness::ActionTypeField;
    m_fieldsByColumn[QnBusiness::TargetColumn] = QnBusiness::ActionTypeField | QnBusiness::ActionParamsField | QnBusiness::ActionResourcesField;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    connect(soundModel, SIGNAL(listLoaded()), this, SLOT(at_soundModel_listChanged()));
    connect(soundModel, SIGNAL(listUnloaded()), this, SLOT(at_soundModel_listChanged()));
    connect(soundModel, SIGNAL(itemChanged(QString)), this, SLOT(at_soundModel_itemChanged(QString)));
    connect(soundModel, SIGNAL(itemRemoved(QString)), this, SLOT(at_soundModel_itemChanged(QString)));
}

QnBusinessRulesViewModel::~QnBusinessRulesViewModel() {

}

QModelIndex QnBusinessRulesViewModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column, (void*)0) : QModelIndex();
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
    Q_UNUSED(parent);
    return QnBusiness::allColumns().size();
}

QVariant QnBusinessRulesViewModel::data(const QModelIndex &index, int role) const {
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();
    return m_rules[index.row()]->data(index.column(), role);
}

bool QnBusinessRulesViewModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;
    return m_rules[index.row()]->setData(index.column(), value, role);
}

QVariant QnBusinessRulesViewModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return QVariant();

    auto validateSize = [](const QSize &size) {
        return size.isValid() ? qVariantFromValue(size) : QVariant() ;
    };

    QnBusiness::Columns column = static_cast<QnBusiness::Columns>(section);

    switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
            return columnTitle(column);
        case Qt::SizeHintRole:
            return validateSize(columnSizeHint(column));
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
            if (QnBusiness::isResourceRequired(m_rules[index.row()]->eventType()))
                flags |= Qt::ItemIsEditable;
            break;
        case QnBusiness::TargetColumn:
            {
                QnBusiness::ActionType actionType = m_rules[index.row()]->actionType();
                if (QnBusiness::requiresCameraResource(actionType)
                        || QnBusiness::requiresUserResource(actionType)
                        || actionType == QnBusiness::ShowPopupAction
                        || actionType == QnBusiness::PlaySoundAction
                        || actionType == QnBusiness::PlaySoundOnceAction
                        || actionType == QnBusiness::SayTextAction)
                    flags |= Qt::ItemIsEditable;
            }
            break;
        case QnBusiness::AggregationColumn:
            {
                QnBusiness::ActionType actionType = m_rules[index.row()]->actionType();
                if (!QnBusiness::hasToggleState(actionType))
                    flags |= Qt::ItemIsEditable;
            }
        default:
            break;
    }
    if (m_rules[index.row()]->system())
        flags &= ~Qt::ItemIsEditable;

    return flags;
}

QString QnBusinessRulesViewModel::columnTitle(QnBusiness::Columns column) const {
    switch (column) {
    case QnBusiness::ModifiedColumn:    
        return tr("#");
    case QnBusiness::DisabledColumn:    
        return tr("On");
    case QnBusiness::EventColumn:       
        return tr("Event");
    case QnBusiness::SourceColumn:      
        return tr("Source");
    case QnBusiness::SpacerColumn:      
        return tr("->");
    case QnBusiness::ActionColumn:      
        return tr("Action");
    case QnBusiness::TargetColumn:      
        return tr("Target");
    case QnBusiness::AggregationColumn: 
        return tr("Interval of Action");
    }
    return QString();
}

void QnBusinessRulesViewModel::forceColumnMinWidth(QnBusiness::Columns column, int width) {
    m_forcedWidthByColumn[column] = width;
}

QSize QnBusinessRulesViewModel::columnSizeHint(QnBusiness::Columns column) const {
    if (!m_forcedWidthByColumn.contains(column))
        return QSize();
    return QSize(m_forcedWidthByColumn[column], 1);
}

void QnBusinessRulesViewModel::addRuleModelInternal(QnBusinessRuleViewModel* ruleModel) {
    connect(ruleModel, SIGNAL(dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)),
        this, SLOT(at_rule_dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)));

    int row = m_rules.size();
    beginInsertRows(QModelIndex(), row, row);
    m_rules << ruleModel;
    endInsertRows();

    emit dataChanged(index(row, 0), index(row, QnBusiness::allColumns().last()));
}

void QnBusinessRulesViewModel::clear() {
    beginResetModel();
    m_rules.clear();
    endResetModel();
}

void QnBusinessRulesViewModel::createRule() {
    QnBusinessRuleViewModel* ruleModel = new QnBusinessRuleViewModel(this);
    ruleModel->setModified(true);
    addRuleModelInternal(ruleModel);
}

void QnBusinessRulesViewModel::addOrUpdateRule(const QnBusinessEventRulePtr &rule) {
    Q_ASSERT_X(rule, Q_FUNC_INFO, "Rule must exist");
    if (!rule)
        return;

    /* System rules are not to be added into any visual models. */
    if (rule->isSystem())
        return;

    QnBusinessRuleViewModel* ruleModel = ruleModelById(rule->id());
    if (!ruleModel) {
        ruleModel = new QnBusinessRuleViewModel(this);
        addRuleModelInternal(ruleModel);
    }
    ruleModel->loadFromRule(rule);
}

void QnBusinessRulesViewModel::deleteRule(QnBusinessRuleViewModel *ruleModel) {
    if (!ruleModel)
        return;

    int row = m_rules.indexOf(ruleModel);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_rules.removeAt(row);
    endRemoveRows();

    //TODO: #GDM #Business check if dataChanged is required, check row
    //emit dataChanged(index(row, 0), index(row, QnBusiness::allColumns().last()));
}

void QnBusinessRulesViewModel::deleteRule(const QnUuid& id) {
    deleteRule(ruleModelById(id));
}

QnBusinessRuleViewModel* QnBusinessRulesViewModel::getRuleModel(int row) {
    if (row < 0 || row >= m_rules.size())
        return NULL;
    return m_rules[row];
}

QnBusinessRuleViewModel* QnBusinessRulesViewModel::ruleModelById(const QnUuid& id) {
    foreach (QnBusinessRuleViewModel* rule, m_rules) {
        if (rule->id() == id) {
            return rule;
        }
    }
    return NULL;
}

void QnBusinessRulesViewModel::at_rule_dataChanged(QnBusinessRuleViewModel *source, QnBusiness::Fields fields) {
    int row = m_rules.indexOf(source);
    if (row < 0)
        return;

    int leftMostColumn = -1;
    int rightMostColumn = QnBusiness::allColumns().last();

    for (QnBusiness::Columns column: QnBusiness::allColumns()) {
        if (fields & m_fieldsByColumn[column]) {
            if (leftMostColumn < 0)
                leftMostColumn = column;
            else
                rightMostColumn = column;
        }
    }

    if (leftMostColumn < 0)
        return;

    QModelIndex index = this->index(row, leftMostColumn, QModelIndex());
    emit dataChanged(index, index.sibling(index.row(), rightMostColumn));
}

void QnBusinessRulesViewModel::at_soundModel_listChanged() {
    for (int i = 0; i < m_rules.size(); i++) {
        if (m_rules[i]->actionType() != QnBusiness::PlaySoundAction &&
                m_rules[i]->actionType() != QnBusiness::PlaySoundOnceAction)
            continue;
        QModelIndex index = this->index(i, QnBusiness::TargetColumn, QModelIndex());
        emit dataChanged(index, index);
    }
}

void QnBusinessRulesViewModel::at_soundModel_itemChanged(const QString &filename) {
    for (int i = 0; i < m_rules.size(); i++) {
        if (m_rules[i]->actionType() != QnBusiness::PlaySoundAction &&
                m_rules[i]->actionType() != QnBusiness::PlaySoundOnceAction)
            continue;
        if (m_rules[i]->actionParams().soundUrl != filename)
            continue;
        QModelIndex index = this->index(i, QnBusiness::TargetColumn, QModelIndex());
        emit dataChanged(index, index);
    }
}
