#ifndef SYSTEM_HEALTH_POPUP_WIDGET_H
#define SYSTEM_HEALTH_POPUP_WIDGET_H

#include <QWidget>

#include <health/system_health.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnSystemHealthPopupWidget;
}

class QnSystemHealthPopupWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    
public:
    explicit QnSystemHealthPopupWidget(QWidget *parent = 0);
    ~QnSystemHealthPopupWidget();
    
    bool showSystemHealthMessage(QnSystemHealth::MessageType message);
signals:
    void closed(QnSystemHealth::MessageType message, bool ignore);

private slots:
    void at_fixButton_clicked();
    void at_postponeButton_clicked();

private:
    QScopedPointer<Ui::QnSystemHealthPopupWidget> ui;

    QnSystemHealth::MessageType m_messageType;
};

#endif // SYSTEM_HEALTH_POPUP_WIDGET_H
