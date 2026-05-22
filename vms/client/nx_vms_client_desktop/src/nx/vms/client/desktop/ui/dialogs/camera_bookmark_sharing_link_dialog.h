// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class CameraBookmarkSharingLinkDialog; }

namespace nx::vms::client::desktop {

class CameraBookmarkSharingLinkDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    explicit CameraBookmarkSharingLinkDialog(
        const common::CameraBookmark& bookmark,
        const QnVirtualCameraResourcePtr& camera,
        QWidget* parent);

    virtual ~CameraBookmarkSharingLinkDialog() override;

signals:
    void stopSharingClicked();
    void editClicked();

private:
    struct Private;
    nx::utils::ImplPtr<Ui::CameraBookmarkSharingLinkDialog> ui;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
