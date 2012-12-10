#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>

namespace Ui {
    class QnBusinessRuleWidget;
}

class QStateMachine;

class QnBusinessRuleWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnBusinessRuleWidget(QWidget *parent = 0);
    ~QnBusinessRuleWidget();

    bool getExpanded();
    void setExpanded(bool expanded = true);
public slots:
    void at_expandButton_clicked();
    void at_deleteButton_clicked();

private slots:
    void updateDisplay();

private:
    Ui::QnBusinessRuleWidget *ui;

    QStateMachine *machine;
    bool m_expanded;
};

#endif // BUSINESS_RULE_WIDGET_H
