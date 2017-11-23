#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QnUserProfileWidget;
class QnUserSettingsWidget;
class QnPermissionsWidget;
class QnAccessibleResourcesWidget;
class QnAbstractPermissionsModel;
class QnUserSettingsModel;
class QnAlertBar;

namespace Ui {
class CameraSettingsDialog;
}

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsModel;

class CameraSettingsDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT

    typedef QnSessionAwareTabbedDialog base_type;

public:
    explicit CameraSettingsDialog(QWidget *parent = NULL);
    virtual ~CameraSettingsDialog();

    bool setCameras(const QnVirtualCameraResourceList &cameras, bool force = false);

protected:
    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;

private:
    Q_DISABLE_COPY(CameraSettingsDialog)
    QScopedPointer<Ui::CameraSettingsDialog> ui;
    QScopedPointer<CameraSettingsModel> m_model;
};

} // namespace desktop
} // namespace client
} // namespace nx

