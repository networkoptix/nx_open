#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnResourceTreeModel;

namespace Ui
{
class CameraPasswordChangeDialog;
} // namespace Ui

class QnCameraPasswordChangeDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    QnCameraPasswordChangeDialog(
        const QnVirtualCameraResourceList& cameras,
        QWidget* parent = nullptr);

    virtual ~QnCameraPasswordChangeDialog();
private:
    QScopedPointer<Ui::CameraPasswordChangeDialog> ui;
};
