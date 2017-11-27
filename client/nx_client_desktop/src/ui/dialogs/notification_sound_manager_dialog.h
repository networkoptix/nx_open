#pragma once

#include <ui/workbench/workbench_context_aware.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class QnNotificationSoundManagerDialog;
}

class QnNotificationSoundModel;

class QnNotificationSoundManagerDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    using base_type = QnSessionAwareButtonBoxDialog;
public:
    explicit QnNotificationSoundManagerDialog(QWidget *parent);
    ~QnNotificationSoundManagerDialog();
    private slots:
    void enablePlayButton();

    void at_playButton_clicked();
    void at_addButton_clicked();
    void at_renameButton_clicked();
    void at_deleteButton_clicked();
private:
    QScopedPointer<Ui::QnNotificationSoundManagerDialog> ui;
    QnNotificationSoundModel* m_model;
};
