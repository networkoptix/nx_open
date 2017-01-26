#include "ptz_messages.h"

#include <ui/dialogs/common/message_box.h>
#include <client/client_settings.h>

void QnPtzMessages::failedToGetPosition(
    QWidget* parent,
    const QString& cameraName)
{
    const auto extras =
        tr("Can't get the current position from camera \"%1\"").arg(cameraName)
        + L'\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to get current position"), extras);
}

void QnPtzMessages::failedToSetPosition(
    QWidget* parent,
    const QString& cameraName)
{
    const auto extras =
        tr("Can't set the current position for camera \"%1\"").arg(cameraName)
        + L'\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to set current position"), extras);
}

bool QnPtzMessages::confirmDeleteUsedPresed(QWidget* parent)
{
    Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
    if (messagesFilter.testFlag(Qn::ShowOnceMessage::PtzPresetInUse))
        return true;

    QnMessageBox dialog(QnMessageBoxIcon::Warning,
        tr("Preset used by some tours. Delete it anyway?"),
        tr("These tours will become invalid."),
        QDialogButtonBox::Cancel, QDialogButtonBox::Yes, parent);

    dialog.addCustomButton(QnMessageBoxCustomButton::Delete);
    dialog.setCheckBoxText(tr("Don't show this message again"));

    const auto result = (dialog.exec() != QDialogButtonBox::Cancel);
    if (dialog.isChecked())
    {
        messagesFilter |= Qn::ShowOnceMessage::PtzPresetInUse;
        qnSettings->setShowOnceMessages(messagesFilter);
        qnSettings->save();
    }
    return result;
}
