#ifndef SENDMAIL_BUSINESS_ACTION_WIDGET_H
#define SENDMAIL_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class SendmailBusinessActionWidget;
}

class QnSendmailBusinessActionWidget : public QnAbstractBusinessParamsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnSendmailBusinessActionWidget(QWidget *parent = 0);
    ~QnSendmailBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;
protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::SendmailBusinessActionWidget> ui;
};

#endif // SENDMAIL_BUSINESS_ACTION_WIDGET_H
