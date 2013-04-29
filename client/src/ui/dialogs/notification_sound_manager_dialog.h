#ifndef NOTIFICATION_SOUND_MANAGER_DIALOG_H
#define NOTIFICATION_SOUND_MANAGER_DIALOG_H

#include <QDialog>

namespace Ui {
    class QnNotificationSoundManagerDialog;
}

class QnNotificationSoundManagerDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit QnNotificationSoundManagerDialog(QWidget *parent = 0);
    ~QnNotificationSoundManagerDialog();
    
private:
   QScopedPointer<Ui::QnNotificationSoundManagerDialog> ui;
};

#endif // NOTIFICATION_SOUND_MANAGER_DIALOG_H
