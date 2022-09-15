// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class FullscreenCameraActionWidget; }

class QnFullscreenCameraActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnFullscreenCameraActionWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent = nullptr);
    virtual ~QnFullscreenCameraActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    void updateCameraButton();
    void updateLayoutButton();
    void updateWarningLabel();
    void openCameraSelectionDialog();
    void openLayoutSelectionDialog();

    void paramsChanged();

private:
    QScopedPointer<Ui::FullscreenCameraActionWidget> ui;
};
