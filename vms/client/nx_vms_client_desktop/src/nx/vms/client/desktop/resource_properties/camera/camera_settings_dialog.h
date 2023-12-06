// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class CameraSettingsDialog; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;

// TODO: #sivanov Implement independent inheritance from WindowContextAware and SystemContextAware
// to be able to open Camera Settings for cross-system cameras.
class CameraSettingsDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareTabbedDialog;

public:
    CameraSettingsDialog(SystemContext* systemContext, QWidget* parent = nullptr);
    virtual ~CameraSettingsDialog() override;

    bool setCameras(const QnVirtualCameraResourceList& cameras, bool force = false);

    const CameraSettingsDialogState& state() const;

    virtual void done(int result) override;

    virtual bool event(QEvent* event) override;

protected:
    virtual void showEvent(QShowEvent* event) override;

    /**
     * Refresh ui state based on model data. Should be called from derived classes.
     */
    virtual void loadDataToUi();

    /**
     * Apply changes to model. Called when user presses OK or Apply buttons.
     */
    virtual void applyChanges();

    /**
     * Discard unsaved changes.
     */
    virtual void discardChanges();

    /**
     * @return Whether all values are correct so saving is possible.
     */
    virtual bool canApplyChanges() const;

    /** Whether dialog has running network request. */
    virtual bool isNetworkRequestRunning() const;

    /**
     * @return Whether there are any unsaved changes in the dialog.
     */
    virtual bool hasChanges() const;

private:
    bool switchCamerasWithConfirmation();

    void loadState(const CameraSettingsDialogState& state);

    void updateScheduleAlert(const CameraSettingsDialogState& state);

private:
    Q_DISABLE_COPY(CameraSettingsDialog)
    QScopedPointer<Ui::CameraSettingsDialog> ui;

    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
