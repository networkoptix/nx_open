#ifndef POPUP_WIDGET_H
#define POPUP_WIDGET_H

#include <QWidget>
#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>

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
    void initAction(const QnAbstractBusinessActionPtr& businessAction);
    void updateDetails(const QnAbstractBusinessActionPtr& businessAction);
    void showSingle();
    void showMultiple();

    void updateCameraDetails(const QnAbstractBusinessActionPtr& businessAction);
    void updateConflictECDetails(const QnAbstractBusinessActionPtr& businessAction);

private:
    Ui::QnPopupWidget *ui;

    BusinessEventType::Value m_eventType;
    int m_eventCount;
    QString m_eventTime;
    QList<QWidget*> m_headerLabels;
    QMap<QString, int> m_resourcesCount;
};

#endif // POPUP_WIDGET_H
