// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class AnalyticsSdkEventWidget; }

namespace nx::vms::client::desktop {
namespace ui {

class AnalyticsSdkEventModel;

class AnalyticsSdkEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit AnalyticsSdkEventWidget(
        SystemContext* systemContext,
        QWidget* parent = nullptr);
    ~AnalyticsSdkEventWidget();

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();

private:
    void updateSdkEventTypesModel();
    void updateSelectedEventType();
    nx::vms::event::EventParameters createEventParameters(
        const QnUuid& engineId,
        const QString& analyticsEventTypeId);

private:
    QScopedPointer<Ui::AnalyticsSdkEventWidget> ui;
    AnalyticsSdkEventModel* m_sdkEventModel;

};

} // namespace ui
} // namespace nx::vms::client::desktop
