#ifndef POPUP_WIDGET_H
#define POPUP_WIDGET_H

#include <QWidget>
#include <events/abstract_business_action.h>

namespace Ui {
    class QnPopupWidget;
}

class QnPopupWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnPopupWidget(QWidget *parent = 0);
    ~QnPopupWidget();
    
    void addBusinessAction(const QnAbstractBusinessActionPtr& businessAction) { m_actionType = businessAction->actionType(); }

signals:
    void closed(BusinessActionType::Value actionType);

private slots:
    void at_okButton_clicked();

private:
    Ui::QnPopupWidget *ui;

    BusinessActionType::Value m_actionType;
};

#endif // POPUP_WIDGET_H
