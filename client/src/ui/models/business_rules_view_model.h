#ifndef BUSINESS_RULES_VIEW_MODEL_H
#define BUSINESS_RULES_VIEW_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <business/business_fwd.h>

#include <ui/models/business_rule_view_model.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/id.h>

class QnBusinessRulesViewModel : public QAbstractItemModel, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QAbstractItemModel base_type;
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
    void addRules(const QnBusinessEventRuleList &businessRules);
    void addRule(QnBusinessEventRulePtr rule);

    void updateRule(QnBusinessEventRulePtr rule);

    void deleteRule(QnBusinessRuleViewModel* ruleModel);
    void deleteRule(const QUuid &id);

    QnBusinessRuleViewModel* getRuleModel(int row);
protected:
    QnBusinessRuleViewModel* ruleModelById(const QUuid &id);

private slots:
    void at_rule_dataChanged(QnBusinessRuleViewModel* source, QnBusiness::Fields fields);
    void at_soundModel_listChanged();
    void at_soundModel_itemChanged(const QString &filename);

private:
    QList<QnBusinessRuleViewModel *> m_rules;
    QMap<int, QnBusiness::Fields> m_fieldsByColumn;
};



#endif // BUSINESS_RULES_VIEW_MODEL_H
