#ifndef SENDMAIL_BUSINESS_ACTION_WIDGET_H
#define SENDMAIL_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnSendmailBusinessActionWidget;
}

class QnSendmailBusinessActionWidget : public QnAbstractBusinessParamsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnSendmailBusinessActionWidget(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnSendmailBusinessActionWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;
private slots:
    void at_settingsButton_clicked();
private:
    Ui::QnSendmailBusinessActionWidget *ui;
};

#endif // SENDMAIL_BUSINESS_ACTION_WIDGET_H
