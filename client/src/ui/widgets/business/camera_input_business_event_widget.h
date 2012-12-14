#ifndef CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
#define CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_event_widget.h>

namespace Ui {
class QnCameraInputBusinessEventWidget;
}

class QnCameraInputBusinessEventWidget : public QnAbstractBusinessEventWidget
{
    Q_OBJECT
    
public:
    explicit QnCameraInputBusinessEventWidget(QWidget *parent = 0);
    ~QnCameraInputBusinessEventWidget();
    
private:
    Ui::QnCameraInputBusinessEventWidget *ui;
};

#endif // CAMERA_INPUT_BUSINESS_EVENT_WIDGET_H
