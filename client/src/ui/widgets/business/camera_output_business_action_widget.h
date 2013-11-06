#ifndef CAMERA_OUTPUT_BUSINESS_ACTION_WIDGET_H
#define CAMERA_OUTPUT_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnCameraOutputBusinessActionWidget;
}

class QnCameraOutputBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnCameraOutputBusinessActionWidget(QWidget *parent = 0);
    ~QnCameraOutputBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::QnCameraOutputBusinessActionWidget> ui;
};

#endif // CAMERA_OUTPUT_BUSINESS_ACTION_WIDGET_H
