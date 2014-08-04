#ifndef NOTIFICATION_SOUND_MANAGER_DIALOG_H
#define NOTIFICATION_SOUND_MANAGER_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class QnNotificationSoundManagerDialog;
}

class QnAppServerNotificationCache;

class QnNotificationSoundManagerDialog : public QnWorkbenchStateDependentButtonBoxDialog {
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

    Q_OBJECT
public:
    explicit QnNotificationSoundManagerDialog(QWidget *parent = 0);
    ~QnNotificationSoundManagerDialog();
private slots:
    void enablePlayButton();

    void at_playButton_clicked();
    void at_addButton_clicked();
    void at_renameButton_clicked();
    void at_deleteButton_clicked();
private:
    QScopedPointer<Ui::QnNotificationSoundManagerDialog> ui;
};

#endif // NOTIFICATION_SOUND_MANAGER_DIALOG_H
