#ifndef POPUP_WIDGET_H
#define POPUP_WIDGET_H

#include <QWidget>
#include <events/abstract_business_event.h>
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
    
    void addBusinessAction(const QnAbstractBusinessActionPtr& businessAction);

signals:
    void closed(BusinessEventType::Value eventType, bool ignore);

private slots:
    void at_okButton_clicked();

private:
    void showSingleAction(const QnAbstractBusinessActionPtr& businessAction);
    void showMultipleActions(const QnAbstractBusinessActionPtr& businessAction);

private:
    Ui::QnPopupWidget *ui;

    BusinessEventType::Value m_eventType;
    int m_eventCount;
    QString m_eventTime;
    QList<QWidget*> m_headerLabels;
};

#endif // POPUP_WIDGET_H
