// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/common/dialogs/generic_tabbed_dialog.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

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

    const CameraSettingsDialogState& state() const;

    virtual void done(int result) override;

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    QDialogButtonBox::StandardButton showConfirmationDialog();

    void loadState(const CameraSettingsDialogState& state);

    void updateButtonsAvailability(const CameraSettingsDialogState& state);
    void updateScheduleAlert(const CameraSettingsDialogState& state);

private:
    Q_DISABLE_COPY(CameraSettingsDialog)
    QScopedPointer<Ui::CameraSettingsDialog> ui;

    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
