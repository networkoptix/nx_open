#ifndef POPUP_BUSINESS_ACTION_WIDGET_H
#define POPUP_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPopupBusinessActionWidget;
}

class QnPopupBusinessActionWidget : public QnAbstractBusinessParamsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnPopupBusinessActionWidget(QWidget *parent = 0);
    ~QnPopupBusinessActionWidget();
    
protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();
    void at_settingsButton_clicked();

private:
    QScopedPointer<Ui::QnPopupBusinessActionWidget> ui;
};

#endif // POPUP_BUSINESS_ACTION_WIDGET_H
