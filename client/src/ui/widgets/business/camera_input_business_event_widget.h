#ifndef CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
#define CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnCameraInputBusinessEventWidget;
}

class QnCameraInputBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnCameraInputBusinessEventWidget(QWidget *parent = 0);
    ~QnCameraInputBusinessEventWidget();

protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::QnCameraInputBusinessEventWidget> ui;
};

#endif // CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
