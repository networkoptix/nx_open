// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_messages.h"

#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <ui/dialogs/common/message_box.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace messages {

void Ptz::failedToGetPosition(QWidget* parent, const QString& cameraName)
{
    const auto extras =
        tr("Cannot get the current position from camera \"%1\"").arg(cameraName)
        + '\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to get current position"), extras);
}

void Ptz::failedToSetPosition(QWidget* parent, const QString& cameraName)
{
    const auto extras =
        tr("Cannot set the current position for camera \"%1\"").arg(cameraName)
        + '\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to set current position"), extras);
}

bool Ptz::deletePresetInUse(QWidget* parent)
{
    if (showOnceSettings()->ptzPresetInUse())
        return true;

    QnMessageBox dialog(QnMessageBoxIcon::Warning,
        tr("Preset used by some tours. Delete it anyway?"),
        tr("These tours will become invalid."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parent);

    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    dialog.setCheckBoxEnabled();

    const auto result = (dialog.exec() != QDialogButtonBox::Cancel);
    if (dialog.isChecked())
        showOnceSettings()->ptzPresetInUse = true;

    return result;
}

} // namespace messages
} // namespace ui
} // namespace nx::vms::client::desktop
