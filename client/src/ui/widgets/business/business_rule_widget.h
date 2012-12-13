#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>
#include <events/business_event_rule.h>

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

    bool getExpanded();
    void setExpanded(bool expanded = true);

    void initAnimations();
    void initEventTypes();
    void initActionTypes();
public slots:
    void at_expandButton_clicked();
    void at_deleteButton_clicked();
    void at_eventTypeComboBox_currentIndexChanged(int index);

private slots:
    void updateDisplay();

private:
    Ui::QnBusinessRuleWidget *ui;

    QStateMachine *machine;
    bool m_expanded;

    QnBusinessEventRulePtr m_rule;

    QStandardItemModel *m_eventsModel;
};

#endif // BUSINESS_RULE_WIDGET_H
