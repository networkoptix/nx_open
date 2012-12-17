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
    

    virtual void loadParameters(const QnBusinessParams &params) override {}
    virtual QnBusinessParams parameters() override {return QnBusinessParams(); }
private:
    Ui::QnCameraInputBusinessEventWidget *ui;
};

#endif // CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
