#ifndef BUSINESS_RULES_VIEW_MODEL_H
#define BUSINESS_RULES_VIEW_MODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QList>

#include <core/resource/resource_fwd.h>

#include <events/business_event_rule.h>
#include <events/business_logic_common.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/qnid.h>

class QnBusinessRuleViewModel: public QObject {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnBusinessRuleViewModel(QObject *parent = 0);

    QVariant data(const int column, const int role) const;

    void loadFromRule(QnBusinessEventRulePtr businessRule);

signals:
    void dataChanged(QnBusinessRuleViewModel* source, int left, int right);

private:
    QVariant getText(const int column) const;
    QVariant getIcon(const int column) const;

private:
    QnId m_id;
    bool m_modified;

    BusinessEventType::Value m_eventType;
    QnResourceList m_eventResources;
    QnBusinessParams m_eventParams;
    ToggleState::Value m_eventState;

    BusinessActionType::Value m_actionType;
    QnResourceList m_actionResources;
    QnBusinessParams m_actionParams;

    int m_aggregationPeriod;
};

class QnBusinessRulesViewModel : public QAbstractItemModel, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QAbstractItemModel base_type;
public:
    explicit QnBusinessRulesViewModel(QObject *parent = 0, QnWorkbenchContext *context = NULL);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void clear();
    void addRules(const QnBusinessEventRules &businessRules);

private:
    QList<QnBusinessRuleViewModel *> m_rules;
    
};

#endif // BUSINESS_RULES_VIEW_MODEL_H
