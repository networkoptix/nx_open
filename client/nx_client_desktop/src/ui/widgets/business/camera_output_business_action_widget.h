#pragma once

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class CameraOutputBusinessActionWidget;
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
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::CameraOutputBusinessActionWidget> ui;
};
