#ifndef SENDMAIL_BUSINESS_ACTION_WIDGET_H
#define SENDMAIL_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnSendmailBusinessActionWidget;
}

class QnSendmailBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnSendmailBusinessActionWidget(QWidget *parent = 0);
    ~QnSendmailBusinessActionWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;
private:
    Ui::QnSendmailBusinessActionWidget *ui;
};

#endif // SENDMAIL_BUSINESS_ACTION_WIDGET_H
