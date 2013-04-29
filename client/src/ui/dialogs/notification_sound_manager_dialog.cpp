#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnNotificationSoundManagerDialog)
{
    ui->setupUi(this);
}

QnNotificationSoundManagerDialog::~QnNotificationSoundManagerDialog()
{
}
