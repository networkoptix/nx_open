// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

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
        const QString& password,
        const QnVirtualCameraResourceList& cameras,
        bool forceShowCamerasList,
        QWidget* parent = nullptr);

    virtual ~QnCameraPasswordChangeDialog();

    QString password() const;

 public:
    virtual void accept() override;

private:
    QScopedPointer<Ui::CameraPasswordChangeDialog> ui;
};
