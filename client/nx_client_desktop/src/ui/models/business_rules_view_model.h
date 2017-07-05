#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <nx/vms/event/event_fwd.h>

#include <ui/models/business_rule_view_model.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>
#include <utils/common/connective.h>

class QnBusinessRulesViewModel : public Connective<QAbstractItemModel>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QAbstractItemModel> base_type;

public:
    using Field = QnBusinessRuleViewModel::Field;
    using Fields = QnBusinessRuleViewModel::Fields;
    using Column = QnBusinessRuleViewModel::Column;

public:
    explicit QnBusinessRulesViewModel(QObject *parent = 0);
    virtual ~QnBusinessRulesViewModel();

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void clear();

    /**
     * @brief           Create new rule.
     * @returns         Row where the rule was added.
     */
    int createRule();

    /** Add existing rule to the model. */
    void addOrUpdateRule(const nx::vms::event::RulePtr &rule);

    void deleteRule(const QnBusinessRuleViewModelPtr &ruleModel);

    void forceColumnMinWidth(Column column, int width);

    QnBusinessRuleViewModelPtr rule(const QModelIndex &index) const;

    QnBusinessRuleViewModelPtr ruleModelById(const QnUuid &id) const;

private:
    bool isIndexValid(const QModelIndex &index) const;

    QString columnTitle(Column column) const;
    QSize columnSizeHint(Column column) const;

    /**
     * @brief           Add new rule to the list.
     * @returns         Row where the rule was added.
     */
    int addRuleModelInternal(const QnBusinessRuleViewModelPtr &ruleModel);

private slots:
    void at_rule_dataChanged(const QnUuid &id, QnBusinessRuleViewModel::Fields fields);
    void at_soundModel_listChanged();
    void at_soundModel_itemChanged(const QString &filename);

private:
    QList<QnBusinessRuleViewModelPtr> m_rules;
    QMap<Column, Fields> m_fieldsByColumn;
    QMap<Column, int> m_forcedWidthByColumn;
};
