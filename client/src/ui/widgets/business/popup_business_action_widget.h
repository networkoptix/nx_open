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
    explicit QnPopupBusinessActionWidget(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnPopupBusinessActionWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;
private:
    Ui::QnPopupBusinessActionWidget *ui;
};

#endif // POPUP_BUSINESS_ACTION_WIDGET_H
