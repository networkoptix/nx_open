#ifndef RECORDING_BUSINESS_ACTION_WIDGET_H
#define RECORDING_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnRecordingBusinessActionWidget;
}

class QnRecordingBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnRecordingBusinessActionWidget(QWidget *parent = 0);
    ~QnRecordingBusinessActionWidget();
    
protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::QnRecordingBusinessActionWidget> ui;
};

#endif // RECORDING_BUSINESS_ACTION_WIDGET_H
