// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <core/misc/schedule_task.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/menu/actions.h>

namespace Ui { class CameraScheduleWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class LicenseUsageProvider;
class RecordScheduleCellPainter;

class CameraScheduleWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraScheduleWidget(
        LicenseUsageProvider* licenseUsageProvider,
        CameraSettingsDialogStore* store,
        QWidget* parent = nullptr);

    virtual ~CameraScheduleWidget() override;

signals:
    void actionRequested(nx::vms::client::desktop::menu::IDType action);

private:
    void setupUi();

    void loadState(const CameraSettingsDialogState& state);
    void loadAlerts(const CameraSettingsDialogState& state);

    QnScheduleTaskList calculateScheduleTasks() const;
    void updateLicensesButton(const CameraSettingsDialogState& state);

private:
    nx::utils::ImplPtr<Ui::CameraScheduleWidget> ui;
    const std::unique_ptr<RecordScheduleCellPainter> m_scheduleCellPainter;
    QPointer<LicenseUsageProvider> m_licenseUsageProvider;
};

} // namespace nx::vms::client::desktop
