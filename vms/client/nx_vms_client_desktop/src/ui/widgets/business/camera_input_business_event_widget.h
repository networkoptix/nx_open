#ifndef CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
#define CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class CameraInputBusinessEventWidget;
}

class QnCameraInputBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnCameraInputBusinessEventWidget(QWidget *parent = 0);
    ~QnCameraInputBusinessEventWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::CameraInputBusinessEventWidget> ui;
};

#endif // CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
