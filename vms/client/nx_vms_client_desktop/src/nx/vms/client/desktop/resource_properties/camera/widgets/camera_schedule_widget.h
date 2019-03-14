#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>

namespace Ui { class CameraScheduleWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class LicenseUsageProvider;

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
    void actionRequested(nx::vms::client::desktop::ui::action::IDType action);

private:
    void setupUi();

    void loadState(const CameraSettingsDialogState& state);
    void loadAlerts(const CameraSettingsDialogState& state);

    QnScheduleTaskList calculateScheduleTasks() const;

    Q_DISABLE_COPY(CameraScheduleWidget)
    QScopedPointer<Ui::CameraScheduleWidget> ui;
};

} // namespace nx::vms::client::desktop
