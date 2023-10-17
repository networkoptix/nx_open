// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui { class PushNotificationBusinessActionWidget; }
namespace nx::vms::client::desktop { class WindowContext; }

namespace nx::vms::client::desktop {

class Aligner;

class PushNotificationBusinessActionWidget:
    public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit PushNotificationBusinessActionWidget(
        nx::vms::client::desktop::WindowContext* context,
        QWidget* parent = nullptr);
    virtual ~PushNotificationBusinessActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    void parametersChanged();
    void updateCurrentTab();
    void updateLimitInfo(QWidget* textField);

private:
    QScopedPointer<Ui::PushNotificationBusinessActionWidget> ui;
    Aligner* const m_aligner;
};

} // namespace nx::vms::client::desktop
