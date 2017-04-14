#include "business_rules_view_model.h"

#include <QtCore/QFileInfo>

#include <client/client_settings.h>

#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/math/math.h>

using namespace nx::client::desktop;

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

    QnNotificationSoundModel* soundModel = context()->instance<ServerNotificationCache>()->persistentGuiModel();
    connect(soundModel, SIGNAL(listLoaded()), this, SLOT(at_soundModel_listChanged()));
    connect(soundModel, SIGNAL(listUnloaded()), this, SLOT(at_soundModel_listChanged()));
    connect(soundModel, SIGNAL(itemChanged(QString)), this, SLOT(at_soundModel_itemChanged(QString)));
    connect(soundModel, SIGNAL(itemRemoved(QString)), this, SLOT(at_soundModel_itemChanged(QString)));
}

QnBusinessRulesViewModel::~QnBusinessRulesViewModel() {

}

QModelIndex QnBusinessRulesViewModel::index(int row, int column, const QModelIndex &parent) const {
    /* Here rowCount and columnCount are checked */
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return qBetween(0, row, m_rules.size())
        ? createIndex(row, column)
        : QModelIndex();
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
    auto ruleModel = rule(index);
    return ruleModel
        ? ruleModel->data(index.column(), role)
        : QVariant();
}

bool QnBusinessRulesViewModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    auto ruleModel = rule(index);
    if (!ruleModel)
        return false;
    return ruleModel->setData(index.column(), value, role);
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
    if (!isIndexValid(index))
        return flags;

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
                if (QnBusiness::allowsAggregation(actionType))
                    flags |= Qt::ItemIsEditable;
            }
        default:
            break;
    }

    return flags;
}

QString QnBusinessRulesViewModel::columnTitle(QnBusiness::Columns column) const
{
    switch (column)
    {
        case QnBusiness::ModifiedColumn:
            return lit("#");
        case QnBusiness::DisabledColumn:
            return tr("On");
        case QnBusiness::EventColumn:
            return tr("Event");
        case QnBusiness::SourceColumn:
            return tr("Source");
        case QnBusiness::SpacerColumn:
            return lit("->");
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

int QnBusinessRulesViewModel::addRuleModelInternal(const QnBusinessRuleViewModelPtr &ruleModel) {
    QnUuid id = ruleModel->id();
    connect(ruleModel, &QnBusinessRuleViewModel::dataChanged, this, [this, id](QnBusiness::Fields fields) {
        at_rule_dataChanged(id, fields);
    });

    int row = m_rules.size();
    beginInsertRows(QModelIndex(), row, row);
    m_rules << ruleModel;
    endInsertRows();

    emit dataChanged(index(row, 0), index(row, QnBusiness::allColumns().last()));
    return row;
}

void QnBusinessRulesViewModel::clear() {
    beginResetModel();
    m_rules.clear();
    endResetModel();
}

int QnBusinessRulesViewModel::createRule() {
    QnBusinessRuleViewModelPtr ruleModel(new QnBusinessRuleViewModel(this));
    ruleModel->setModified(true);
    return addRuleModelInternal(ruleModel);
}

void QnBusinessRulesViewModel::addOrUpdateRule(const QnBusinessEventRulePtr &rule) {
    NX_ASSERT(rule, Q_FUNC_INFO, "Rule must exist");
    if (!rule)
        return;

    /* System rules are not to be added into any visual models. */
    if (rule->isSystem())
        return;

    QnBusinessRuleViewModelPtr ruleModel = ruleModelById(rule->id());
    if (!ruleModel)
    {
        ruleModel = QnBusinessRuleViewModelPtr(new QnBusinessRuleViewModel(this));
        ruleModel->loadFromRule(rule);
        addRuleModelInternal(ruleModel);
    }
    else
        ruleModel->loadFromRule(rule);
}

void QnBusinessRulesViewModel::deleteRule(const QnBusinessRuleViewModelPtr &ruleModel) {
    if (!ruleModel)
        return;

    int row = m_rules.indexOf(ruleModel);
    if (row < 0)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_rules.removeAt(row);
    endRemoveRows();
}

QnBusinessRuleViewModelPtr QnBusinessRulesViewModel::ruleModelById(const QnUuid& id) const {
    for (QnBusinessRuleViewModelPtr rule: m_rules) {
        if (rule->id() == id)
            return rule;
    }
    return QnBusinessRuleViewModelPtr();
}

bool QnBusinessRulesViewModel::isIndexValid( const QModelIndex &index ) const {
    return index.isValid()
        && index.model() == this
        && hasIndex(index.row(), index.column(), index.parent());
}

QnBusinessRuleViewModelPtr QnBusinessRulesViewModel::rule( const QModelIndex &index ) const {
    if (index.model() == this) {
        return qBetween(0, index.row(), m_rules.size())
            ? m_rules[index.row()]
            : QnBusinessRuleViewModelPtr();
    }

    return ruleModelById(index.data(Qn::UuidRole).value<QnUuid>());
}

void QnBusinessRulesViewModel::at_rule_dataChanged(const QnUuid &id, QnBusiness::Fields fields) {
    auto model = ruleModelById(id);
    if (!model)
        return;

    int row = m_rules.indexOf(model);
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
        if (m_rules[i]->actionParams().url != filename)
            continue;
        QModelIndex index = this->index(i, QnBusiness::TargetColumn, QModelIndex());
        emit dataChanged(index, index);
    }
}
