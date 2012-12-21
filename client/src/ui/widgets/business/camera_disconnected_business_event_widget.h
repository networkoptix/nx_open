#ifndef CAMERA_DISCONNECTED_BUSINESS_EVENT_WIDGET_H
#define CAMERA_DISCONNECTED_BUSINESS_EVENT_WIDGET_H

#include <QWidget>
#include "abstract_business_params_widget.h"

namespace Ui {
class QnCameraDisconnectedBusinessEventWidget;
}

class QnCameraDisconnectedBusinessEventWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    
public:
    explicit QnCameraDisconnectedBusinessEventWidget(QWidget *parent = 0);
    ~QnCameraDisconnectedBusinessEventWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;

private:
    Ui::QnCameraDisconnectedBusinessEventWidget *ui;
};

#endif // CAMERA_DISCONNECTED_BUSINESS_EVENT_WIDGET_H
