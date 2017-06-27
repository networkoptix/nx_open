#include "ptz_messages.h"

#include <ui/dialogs/common/message_box.h>
#include <client/client_show_once_settings.h>

namespace {

/* Delete ptz preset which is used in the tour. */
static const QString kPtzPresetShowOnceKey(lit("PtzPresetInUse"));

} //namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace ptz {

void failedToGetPosition(QWidget* parent, const QString& cameraName)
{
    const auto extras =
        tr("Cannot get the current position from camera \"%1\"").arg(cameraName)
        + L'\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to get current position"), extras);
}

void failedToSetPosition(QWidget* parent, const QString& cameraName)
{
    const auto extras =
        tr("Cannot set the current position for camera \"%1\"").arg(cameraName)
        + L'\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to set current position"), extras);
}

bool deletePresetInUse(QWidget* parent)
{
    if (qnClientShowOnce->testFlag(kPtzPresetShowOnceKey))
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
        qnClientShowOnce->setFlag(kPtzPresetShowOnceKey);

    return result;
}

} // namespace ptz
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
