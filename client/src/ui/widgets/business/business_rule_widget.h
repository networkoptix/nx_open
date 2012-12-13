#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>

#include <events/business_event_rule.h>
#include <events/business_logic_common.h>

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
/*
    bool getExpanded();
    void setExpanded(bool expanded = true);
*/

public slots:
    void at_expandButton_clicked();
    void at_deleteButton_clicked();
    void at_eventTypeComboBox_currentIndexChanged(int index);

protected:
    void initAnimations();

    void initEventTypes();
    void initEventStates(BusinessEventType::Value eventType);
    void initEventParameters(BusinessEventType::Value eventType);

    void initActionTypes();

private slots:
    void updateDisplay();
    void updateSelection();

private:
    Ui::QnBusinessRuleWidget *ui;

    QStateMachine *machine;
    bool m_expanded;

    QnBusinessEventRulePtr m_rule;

    QWidget *m_eventParameters;

    QStandardItemModel *m_eventsTypesModel;
    QStandardItemModel *m_eventStatesModel;
};

#endif // BUSINESS_RULE_WIDGET_H
