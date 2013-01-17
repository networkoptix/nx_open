#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>

#include <events/business_event_rule.h>
#include <events/business_logic_common.h>
#include <events/abstract_business_event.h>

#include <ui/models/business_rules_view_model.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnBusinessRuleWidget;
}

class QStateMachine;
class QStandardItemModel;

class QnBusinessRuleWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    
    typedef QWidget base_type;
public:
    explicit QnBusinessRuleWidget(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnBusinessRuleWidget();

    void setModel(QnBusinessRuleViewModel* model);
protected:
    /**
     * @brief initEventTypes        Fill combobox with all possible event types.
     */
    void initEventTypes();

    /**
     * @brief initEventStates       Fill combobox with event states allowed to current event type.
     * @param eventType             Selected event type.
     */
    void initEventStates(BusinessEventType::Value eventType);

    /**
     * @brief initEventParameters   Display widget with current event paramenters.
     * @param eventType             Selected event type.
     */
    void initEventParameters(BusinessEventType::Value eventType);

    /**
     * @brief initActionTypes       Fill combobox with actions allowed with current event state.
     * @param eventState            Selected event state.
     */
    void initActionTypes(ToggleState::Value eventState);


    void initActionParameters(BusinessActionType::Value actionType);

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private slots:
    void at_eventTypeComboBox_currentIndexChanged(int index);
    void at_eventStatesComboBox_currentIndexChanged(int index);
    void at_actionTypeComboBox_currentIndexChanged(int index);
    void at_actionParametersChanged();

    void at_eventResourcesHolder_clicked();
    void at_actionResourcesHolder_clicked();

    void at_aggregationPeriodChanged();

    void updateEventResources();
    void updateActionResources();
    void updateDefinition();
private:
    BusinessEventType::Value getCurrentEventType() const;
    ToggleState::Value getCurrentEventToggleState() const;

    BusinessActionType::Value getCurrentActionType() const;

    QString getResourceName(const QnResourcePtr& resource) const;
private:
    Ui::QnBusinessRuleWidget *ui;

    QnBusinessRuleViewModel* m_model;

    QnAbstractBusinessParamsWidget *m_eventParameters;
    QnAbstractBusinessParamsWidget *m_actionParameters;

    QMap<BusinessEventType::Value, QnAbstractBusinessParamsWidget*> m_eventWidgetsByType;
    QMap<BusinessActionType::Value, QnAbstractBusinessParamsWidget*> m_actionWidgetsByType;

    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_eventStatesModel;
    QStandardItemModel *m_actionTypesModel;

    QnResourceList m_dropResources;

    QnResourceList m_eventResources;
    QnResourceList m_actionResources;
};

#endif // BUSINESS_RULE_WIDGET_H
