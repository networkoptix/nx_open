// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class AnalyticsSdkObjectDetectedWidget; }

namespace nx::vms::client::desktop {
namespace ui {

class AnalyticsSdkObjectDetectedModel;

class AnalyticsSdkObjectDetectedWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit AnalyticsSdkObjectDetectedWidget(
        SystemContext* systemContext,
        QWidget* parent = nullptr);
    ~AnalyticsSdkObjectDetectedWidget();

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();

private:
    nx::vms::event::EventParameters createEventParameters(
        const QString& analyticsEventTypeId, const QString& attributes);

private:
    QScopedPointer<Ui::AnalyticsSdkObjectDetectedWidget> ui;

};

} // namespace ui
} // namespace nx::vms::client::desktop
