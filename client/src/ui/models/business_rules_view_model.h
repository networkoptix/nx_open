#ifndef BUSINESS_RULES_VIEW_MODEL_H
#define BUSINESS_RULES_VIEW_MODEL_H

#include <QAbstractItemModel>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QList>

#include <core/resource/resource_fwd.h>

#include <events/business_event_rule.h>
#include <events/business_logic_common.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/qnid.h>

namespace QnBusiness {
    enum Columns {
        ModifiedColumn,
        EventColumn,
        SourceColumn,
        SpacerColumn,
        ActionColumn,
        TargetColumn,
        ColumnCount
    };

    enum Field {
        ModifiedField           = 0x00000001,
        EventTypeField          = 0x00000002,
        EventResourcesField     = 0x00000004,
        EventParamsField        = 0x00000008,
        EventStateField         = 0x00000010,
        ActionTypeField         = 0x00000020,
        ActionResourcesField    = 0x00000040,
        ActionParamsField       = 0x00000080,
        AggregationField        = 0x00000100,
        AllFieldsMask           = 0x0000FFFF,

        ActionIsInstantField    = EventTypeField | EventStateField
    };
    Q_DECLARE_FLAGS(Fields, Field)

}


class QnBusinessRuleViewModel: public QObject {
    Q_OBJECT

    typedef QObject base_type;

public:
    QnBusinessRuleViewModel(QObject *parent = 0);

    QVariant data(const int column, const int role = Qt::DisplayRole) const;

    void loadFromRule(QnBusinessEventRulePtr businessRule);
    bool actionTypeShouldBeInstant();
    QnBusinessEventRulePtr createRule() const;

    QVariant getText(const int column, const bool detailed = true) const;
    QVariant getIcon(const int column) const;

    QnId id() const;

    bool isModified() const;
    void setModified(bool value);

    BusinessEventType::Value eventType() const;
    void setEventType(const BusinessEventType::Value value);

    QnResourceList eventResources() const;
    void setEventResources(const QnResourceList &value);

    QnBusinessParams eventParams() const;
    void setEventParams(const QnBusinessParams& params);

    ToggleState::Value eventState() const;
    void setEventState(ToggleState::Value state);

    BusinessActionType::Value actionType() const;
    void setActionType(const BusinessActionType::Value value);

    QnResourceList actionResources() const;
    void setActionResources(const QnResourceList &value);

    QnBusinessParams actionParams() const;
    void setActionParams(const QnBusinessParams& params);

    int aggregationPeriod() const;
    void setAggregationPeriod(int secs);

    QStandardItemModel* eventTypesModel();
    QStandardItemModel* eventStatesModel();
    QStandardItemModel* actionTypesModel();
signals:
    void dataChanged(QnBusinessRuleViewModel* source, QnBusiness::Fields fields);

private:
    void updateEventTypesModel();
    void updateEventStatesModel();
    void updateActionTypesModel();

    QString getTargetText(const bool detailed) const;

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

    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_eventStatesModel;
    QStandardItemModel *m_actionTypesModel;

};

class QnBusinessRulesViewModel : public QAbstractItemModel, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QAbstractItemModel base_type;
public:
    explicit QnBusinessRulesViewModel(QObject *parent = 0, QnWorkbenchContext *context = NULL);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void clear();
    void addRules(const QnBusinessEventRules &businessRules);
    void addRule(QnBusinessEventRulePtr rule);
    void deleteRule(QnBusinessRuleViewModel* ruleModel);

    bool hasModifiedItems() const;

    QnBusinessRuleViewModel* getRuleModel(int row);
private slots:
    void at_rule_dataChanged(QnBusinessRuleViewModel* source, QnBusiness::Fields fields);

private:
    QList<QnBusinessRuleViewModel *> m_rules;
    QMap<int, QnBusiness::Fields> m_fieldsByColumn;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnBusiness::Fields)

#endif // BUSINESS_RULES_VIEW_MODEL_H
