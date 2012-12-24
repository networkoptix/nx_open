#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>

#include <events/business_event_rule.h>
#include <events/business_logic_common.h>
#include <events/abstract_business_event.h>

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
    explicit QnBusinessRuleWidget(QnBusinessEventRulePtr rule, QWidget *parent = 0);
    ~QnBusinessRuleWidget();

    QnBusinessEventRulePtr rule() const;
signals:
    /**
     * @brief deleteConfirmed       Signal is emitted when "Delete" button was clicked and user has confirmed deletion.
     * @param rule                  Rule to delete. Should be replaced by ID if it is convinient.
     */
    void deleteConfirmed(QnBusinessRuleWidget* source, QnBusinessEventRulePtr rule);

    void apply(QnBusinessRuleWidget* source, QnBusinessEventRulePtr rule);

    void hasChangesChanged(QnBusinessRuleWidget* source, bool value);
    void eventTypeChanged(QnBusinessRuleWidget* source, BusinessEventType::Value value);
    void eventResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource);
    void eventStateChanged(QnBusinessRuleWidget* source, ToggleState::Value value);
    void actionTypeChanged(QnBusinessRuleWidget* source, BusinessActionType::Value value);
    void actionResourceChanged(QnBusinessRuleWidget* source, const QnResourcePtr &resource);
protected:
    /**
     * @brief initEventTypes        Fill combobox with all possible event types.
     */
    void initEventTypes();

    void initEventResources(BusinessEventType::Value eventType);

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

    void initActionResources(BusinessActionType::Value actionType);

    void initActionParameters(BusinessActionType::Value actionType);

private slots:
    void at_expandButton_clicked();
    void at_deleteButton_clicked();
    void at_applyButton_clicked();

    void at_eventTypeComboBox_currentIndexChanged(int index);
    void at_eventStatesComboBox_currentIndexChanged(int index);
    void at_eventResourceComboBox_currentIndexChanged(int index);
    void at_actionTypeComboBox_currentIndexChanged(int index);
    void at_actionResourceComboBox_currentIndexChanged(int index);

private slots:
    void resetFromRule();
    void updateResources();

private:
    BusinessEventType::Value getCurrentEventType() const;
    QnResourcePtr getCurrentEventResource() const;
    ToggleState::Value getCurrentEventToggleState() const;

    BusinessActionType::Value getCurrentActionType() const;
    QnResourcePtr getCurrentActionResource() const;

    QString getResourceName(const QnResourcePtr& resource) const;

    void setHasChanges(bool hasChanges);
private:
    Ui::QnBusinessRuleWidget *ui;

    QnBusinessEventRulePtr m_rule;
    bool m_hasChanges;

    QnAbstractBusinessParamsWidget *m_eventParameters;
    QnAbstractBusinessParamsWidget *m_actionParameters;

    QMap<BusinessEventType::Value, QnAbstractBusinessParamsWidget*> m_eventWidgetsByType;
    QMap<BusinessActionType::Value, QnAbstractBusinessParamsWidget*> m_actionWidgetsByType;

    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_eventResourcesModel;
    QStandardItemModel *m_eventStatesModel;
    QStandardItemModel *m_actionTypesModel;
    QStandardItemModel *m_actionResourcesModel;
};

#endif // BUSINESS_RULE_WIDGET_H
