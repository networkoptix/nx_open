#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>

#include <events/business_event_rule.h>
#include <events/business_logic_common.h>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnBusinessRuleWidget;
}

class QStateMachine;
class QStandardItemModel;

class QnBusinessRuleWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnBusinessRuleWidget(QWidget *parent = 0);
    ~QnBusinessRuleWidget();

    void setRule(QnBusinessEventRulePtr rule);
    QnBusinessEventRulePtr getRule() const;

    void setExpanded(bool expanded = true);
    bool expanded() const;

signals:
    /**
     * @brief deleteConfirmed       Signal is emitted when "Delete" button was clicked and user has confirmed deletion.
     * @param rule                  Rule to delete. Should be replaced by ID if it is convinient.
     */
    void deleteConfirmed(QnBusinessEventRulePtr rule);

    void apply(QnBusinessEventRulePtr rule);
protected:
    /**
     * @brief initAnimations        Create all required animations and state machines.
     */
    void initAnimations();

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

private slots:
    void at_expandButton_clicked();
    void at_deleteButton_clicked();
    void at_applyButton_clicked();
    void at_eventTypeComboBox_currentIndexChanged(int index);
    void at_eventStatesComboBox_currentIndexChanged(int index);
    void at_actionTypeComboBox_currentIndexChanged(int index);

private slots:
    void updateDisplay();
    void resetFromRule();
    void updateSummary();

private:
    BusinessEventType::Value getCurrentEventType() const;
    BusinessActionType::Value getCurrentActionType() const;
    ToggleState::Value getCurrentEventToggleState() const;

private:
    Ui::QnBusinessRuleWidget *ui;

    bool m_expanded;

    QnBusinessEventRulePtr m_rule;

    QnAbstractBusinessParamsWidget *m_eventParameters;
    QnAbstractBusinessParamsWidget *m_actionParameters;

    QStandardItemModel *m_eventsTypesModel;
    QStandardItemModel *m_eventStatesModel;
    QStandardItemModel *m_actionTypesModel;
};

#endif // BUSINESS_RULE_WIDGET_H
