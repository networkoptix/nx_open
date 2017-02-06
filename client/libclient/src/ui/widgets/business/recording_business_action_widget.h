#ifndef RECORDING_BUSINESS_ACTION_WIDGET_H
#define RECORDING_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class RecordingBusinessActionWidget;
}

class QnRecordingBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnRecordingBusinessActionWidget(QWidget *parent = 0);
    ~QnRecordingBusinessActionWidget();

     virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::RecordingBusinessActionWidget> ui;
};

#endif // RECORDING_BUSINESS_ACTION_WIDGET_H
