#include "business_rules_view_model.h"

#include <QtCore/QFileInfo>

#include <client/client_settings.h>

#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/math/math.h>

using namespace nx;
using namespace nx::client::desktop;

QnBusinessRulesViewModel::QnBusinessRulesViewModel(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    m_fieldsByColumn[Column::modified] = Field::modified;
    m_fieldsByColumn[Column::disabled] = Field::disabled;
    m_fieldsByColumn[Column::event]    = Field::eventType | Field::eventState | Field::actionType;
    m_fieldsByColumn[Column::source]   = Field::eventType | Field::eventResources;
    m_fieldsByColumn[Column::spacer]   = 0;
    m_fieldsByColumn[Column::action]   = Field::actionType;
    m_fieldsByColumn[Column::target]   = Field::actionType | Field::actionParams | Field::actionResources;

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
    return QnBusinessRuleViewModel::allColumns().size();
}

QVariant QnBusinessRulesViewModel::data(const QModelIndex &index, int role) const {
    auto ruleModel = rule(index);
    return ruleModel
        ? ruleModel->data(Column(index.column()), role)
        : QVariant();
}

bool QnBusinessRulesViewModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    auto ruleModel = rule(index);
    if (!ruleModel)
        return false;
    return ruleModel->setData(Column(index.column()), value, role);
}

QVariant QnBusinessRulesViewModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return QVariant();

    auto validateSize = [](const QSize &size) {
        return size.isValid() ? qVariantFromValue(size) : QVariant() ;
    };

    auto column = static_cast<Column>(section);

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

    switch (static_cast<Column>(index.column()))
    {
        case Column::disabled:
            flags |= Qt::ItemIsUserCheckable;
            break;

        case Column::event:
        case Column::action:
            flags |= Qt::ItemIsEditable;
            break;

        case Column::source:
            if (vms::event::isResourceRequired(m_rules[index.row()]->eventType()))
                flags |= Qt::ItemIsEditable;
            break;

        case Column::target:
        {
            const auto actionType = m_rules[index.row()]->actionType();
            if (vms::event::requiresCameraResource(actionType)
                    || vms::event::requiresUserResource(actionType)
                    || actionType == vms::event::showPopupAction
                    || actionType == vms::event::playSoundAction
                    || actionType == vms::event::playSoundOnceAction
                    || actionType == vms::event::sayTextAction)
                flags |= Qt::ItemIsEditable;
        }
        break;

        case Column::aggregation:
        {
            const auto actionType = m_rules[index.row()]->actionType();
            if (vms::event::allowsAggregation(actionType))
                flags |= Qt::ItemIsEditable;
        }

        default:
            break;
    }

    return flags;
}

QString QnBusinessRulesViewModel::columnTitle(Column column) const
{
    switch (column)
    {
        case Column::modified:    return lit("#");
        case Column::disabled:    return tr("On");
        case Column::event:       return tr("Event");
        case Column::source:      return tr("Source");
        case Column::spacer:      return lit("->");
        case Column::action:      return tr("Action");
        case Column::target:      return tr("Target");
        case Column::aggregation: return tr("Interval of Action");
        default:                  return QString();
    }
}

void QnBusinessRulesViewModel::forceColumnMinWidth(Column column, int width)
{
    m_forcedWidthByColumn[column] = width;
}

QSize QnBusinessRulesViewModel::columnSizeHint(Column column) const
{
    if (!m_forcedWidthByColumn.contains(column))
        return QSize();
    return QSize(m_forcedWidthByColumn[column], 1);
}

int QnBusinessRulesViewModel::addRuleModelInternal(const QnBusinessRuleViewModelPtr& ruleModel)
{
    QnUuid id = ruleModel->id();
    connect(ruleModel, &QnBusinessRuleViewModel::dataChanged, this,
        [this, id](Fields fields)
        {
            at_rule_dataChanged(id, fields);
        });

    int row = m_rules.size();
    beginInsertRows(QModelIndex(), row, row);
    m_rules << ruleModel;
    endInsertRows();

    emit dataChanged(
        index(row, 0),
        index(row, static_cast<int>(QnBusinessRuleViewModel::allColumns().last())));

    return row;
}

void QnBusinessRulesViewModel::clear()
{
    beginResetModel();
    m_rules.clear();
    endResetModel();
}

int QnBusinessRulesViewModel::createRule()
{
    QnBusinessRuleViewModelPtr ruleModel(new QnBusinessRuleViewModel(this));
    ruleModel->setModified(true);
    return addRuleModelInternal(ruleModel);
}

void QnBusinessRulesViewModel::addOrUpdateRule(const vms::event::RulePtr& rule)
{
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
    {
        ruleModel->loadFromRule(rule);
    }
}

void QnBusinessRulesViewModel::deleteRule(const QnBusinessRuleViewModelPtr& ruleModel)
{
    if (!ruleModel)
        return;

    int row = m_rules.indexOf(ruleModel);
    if (row < 0)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_rules.removeAt(row);
    endRemoveRows();
}

QnBusinessRuleViewModelPtr QnBusinessRulesViewModel::ruleModelById(const QnUuid& id) const
{
    for (const auto& rule: m_rules)
    {
        if (rule->id() == id)
            return rule;
    }
    return QnBusinessRuleViewModelPtr();
}

bool QnBusinessRulesViewModel::isIndexValid(const QModelIndex& index) const
{
    return index.isValid()
        && index.model() == this
        && hasIndex(index.row(), index.column(), index.parent());
}

QnBusinessRuleViewModelPtr QnBusinessRulesViewModel::rule(const QModelIndex& index) const
{
    if (index.model() == this)
    {
        return qBetween(0, index.row(), m_rules.size())
            ? m_rules[index.row()]
            : QnBusinessRuleViewModelPtr();
    }

    return ruleModelById(index.data(Qn::UuidRole).value<QnUuid>());
}

void QnBusinessRulesViewModel::at_rule_dataChanged(const QnUuid &id, Fields fields)
{
    auto model = ruleModelById(id);
    if (!model)
        return;

    int row = m_rules.indexOf(model);
    if (row < 0)
        return;

    int leftMostColumn = -1;
    int rightMostColumn = int(QnBusinessRuleViewModel::allColumns().last());

    for (auto column: QnBusinessRuleViewModel::allColumns())
    {
        if (fields & m_fieldsByColumn[column])
        {
            if (leftMostColumn < 0)
                leftMostColumn = int(column);
            else
                rightMostColumn = int(column);
        }
    }

    if (leftMostColumn < 0)
        return;

    QModelIndex index = this->index(row, leftMostColumn, QModelIndex());
    emit dataChanged(index, index.sibling(index.row(), rightMostColumn));
}

void QnBusinessRulesViewModel::at_soundModel_listChanged()
{
    for (int i = 0; i < m_rules.size(); i++)
    {
        if (m_rules[i]->actionType() != vms::event::playSoundAction
            && m_rules[i]->actionType() != vms::event::playSoundOnceAction)
        {
            continue;
        }
        QModelIndex index = this->index(i, int(Column::target), QModelIndex());
        emit dataChanged(index, index);
    }
}

void QnBusinessRulesViewModel::at_soundModel_itemChanged(const QString& filename)
{
    for (int i = 0; i < m_rules.size(); i++)
    {
        if ((m_rules[i]->actionType() != vms::event::playSoundAction
            && m_rules[i]->actionType() != vms::event::playSoundOnceAction)
            || m_rules[i]->actionParams().url != filename)
        {
            continue;
        }

        QModelIndex index = this->index(i, int(Column::target), QModelIndex());
        emit dataChanged(index, index);
    }
}
