// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    explicit QnCameraOutputBusinessActionWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent = nullptr);
    ~QnCameraOutputBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::CameraOutputBusinessActionWidget> ui;
};
