#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QWidget>

namespace Ui {
class QnBusinessRuleWidget;
}

class QnBusinessRuleWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnBusinessRuleWidget(QWidget *parent = 0);
    ~QnBusinessRuleWidget();
    
private:
    Ui::QnBusinessRuleWidget *ui;
};

#endif // BUSINESS_RULE_WIDGET_H
