// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmark_sharing_settings_dialog.h"
#include "ui_camera_bookmark_sharing_settings_dialog.h"

#include <core/resource/camera_bookmark.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

CameraBookmarkSharingSettingsDialog::CameraBookmarkSharingSettingsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraBookmarkSharingSettingsDialog)
{
    ui->setupUi(this);
    ui->passwordInputField->setPlaceholderText(tr("No password protection"));
    ui->passwordInputField->setFocusPolicy(Qt::ClickFocus);

    using namespace std::chrono;

    const auto addDurationOption =
        [this](const QString& title, milliseconds duration)
        {
            ui->lifetimeComboBox->addItem(title, QVariant::fromValue(duration));
        };

    addDurationOption(tr("Expires in an hour"), hours(1));
    addDurationOption(tr("Expires in a day"), days(1));
    addDurationOption(tr("Expires in a month"), days(30));
    addDurationOption(tr("Never expires"), {});
    ui->lifetimeComboBox->setCurrentIndex(1);
}

CameraBookmarkSharingSettingsDialog::~CameraBookmarkSharingSettingsDialog()
{
}

void CameraBookmarkSharingSettingsDialog::setBookmark(const common::CameraBookmark& bookmark)
{
    ui->passwordInputField->setHasRemotePassword(bookmark.share.digest.has_value());
}

std::chrono::milliseconds CameraBookmarkSharingSettingsDialog::expirationTimeMs() const
{
    using namespace std::chrono;

    const auto duration = ui->lifetimeComboBox->currentData().value<milliseconds>();
    if (duration == milliseconds::zero())
        return {};

    return duration + qnSyncTime->value();
}

std::optional<QString> CameraBookmarkSharingSettingsDialog::password() const
{
    if (ui->passwordInputField->hasRemotePassword())
        return {};

    return ui->passwordInputField->text();
}

} // namespace nx::vms::client::desktop
