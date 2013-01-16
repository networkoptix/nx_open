#ifndef CAMERA_OUTPUT_BUSINESS_ACTION_WIDGET_H
#define CAMERA_OUTPUT_BUSINESS_ACTION_WIDGET_H

#include <QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnCameraOutputBusinessActioWidget;
}

class QnCameraOutputBusinessActioWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnCameraOutputBusinessActioWidget(QWidget *parent = 0);
    ~QnCameraOutputBusinessActioWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;
private:
    Ui::QnCameraOutputBusinessActioWidget *ui;
};

#endif // CAMERA_OUTPUT_BUSINESS_ACTION_WIDGET_H
