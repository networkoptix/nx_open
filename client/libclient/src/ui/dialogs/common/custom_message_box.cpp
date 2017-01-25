#include "custom_message_box.h"

#include <ui/dialogs/common/message_box.h>
#include <utils/common/app_info.h>

void QnCustomMessageBox::showFailedToGetPosition(
    QWidget* parent,
    const QString& cameraName)
{
    const auto extras =
        tr("Can't get the current position from camera \"%1\"").arg(cameraName)
        + L'\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to get current position"), extras);

}

void QnCustomMessageBox::showFailedToSetPosition(
    QWidget* parent,
    const QString& cameraName)
{
    const auto extras =
        tr("Can't set the current position for camera \"%1\"").arg(cameraName)
        + L'\n' + tr("Please wait for the camera to go online.");
    QnMessageBox::critical(parent, tr("Failed to set current position"), extras);

}

void QnCustomMessageBox::showFailedRestartClient(QWidget* parent)
{
    QnMessageBox::critical(parent,
        tr("Failed to restart %1 Client").arg(QnAppInfo::productNameLong()),
        tr("Please close the application and start it again using the shortcut in the start menu."));
}

void QnCustomMessageBox::showAnotherVideoWallExist(QWidget* parent)
{
    QnMessageBox::warning(parent, tr("There is another Video Wall with the same name"));
}
