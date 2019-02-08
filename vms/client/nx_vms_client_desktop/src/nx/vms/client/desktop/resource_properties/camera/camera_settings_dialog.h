#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/common/dialogs/generic_tabbed_dialog.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

class QnUserProfileWidget;
class QnUserSettingsWidget;
class QnPermissionsWidget;
class QnAccessibleResourcesWidget;
class QnAbstractPermissionsModel;
class QnUserSettingsModel;
class AlertBar;

namespace Ui { class CameraSettingsDialog; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;

class CameraSettingsDialog:
    public GenericTabbedDialog,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = GenericTabbedDialog;

public:
    explicit CameraSettingsDialog(QWidget* parent = nullptr);
    virtual ~CameraSettingsDialog() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    bool setCameras(const QnVirtualCameraResourceList& cameras, bool force = false);

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    QDialogButtonBox::StandardButton showConfirmationDialog();

    void updateState();

    void updateButtonsAvailability();

private:
    Q_DISABLE_COPY(CameraSettingsDialog)
    QScopedPointer<Ui::CameraSettingsDialog> ui;

    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
