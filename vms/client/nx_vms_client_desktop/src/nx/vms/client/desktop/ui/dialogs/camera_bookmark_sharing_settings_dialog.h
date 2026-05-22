// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class CameraBookmarkSharingSettingsDialog; }
namespace nx::vms::common { struct CameraBookmark; }

namespace nx::vms::client::desktop {

class CameraBookmarkSharingSettingsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit CameraBookmarkSharingSettingsDialog(QWidget* parent);
    virtual ~CameraBookmarkSharingSettingsDialog() override;

    void setBookmark(const nx::vms::common::CameraBookmark& bookmark);

    std::chrono::milliseconds expirationTimeMs() const;
    std::optional<QString> password() const;

private:
    nx::utils::ImplPtr<Ui::CameraBookmarkSharingSettingsDialog> ui;
};

} // namespace nx::vms::client::desktop
