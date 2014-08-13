#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

#include <QtCore/QFileInfo>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>

#include <client/client_settings.h>

#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/media/audio_player.h>

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnNotificationSoundManagerDialog)
{
    ui->setupUi(this);

    ui->listView->setModel(context()->instance<QnAppServerNotificationCache>()->persistentGuiModel());

    connect(ui->playButton,     SIGNAL(clicked()), this, SLOT(at_playButton_clicked()));
    connect(ui->addButton,      SIGNAL(clicked()), this, SLOT(at_addButton_clicked()));
    connect(ui->renameButton,   SIGNAL(clicked()), this, SLOT(at_renameButton_clicked()));
    connect(ui->deleteButton,   SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
}

QnNotificationSoundManagerDialog::~QnNotificationSoundManagerDialog()
{
}

void QnNotificationSoundManagerDialog::enablePlayButton() {
    ui->playButton->setEnabled(true);
}

void QnNotificationSoundManagerDialog::at_playButton_clicked() {
    if (!ui->listView->currentIndex().isValid())
        return;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    QString filename = soundModel->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(filename);
    if (!QFileInfo(filePath).exists())
        return;

    bool result = AudioPlayer::playFileAsync(filePath, this, SLOT(enablePlayButton()));
    if (result)
        ui->playButton->setEnabled(false);
}

void QnNotificationSoundManagerDialog::at_addButton_clicked() {
    //TODO: #GDM #Common progressbar required

    QString supportedFormats = tr("Sound files");
    supportedFormats += QLatin1String(" (*.wav *.mp3 *.ogg *.wma)");

    QScopedPointer<QnCustomFileDialog> dialog(new QnWorkbenchStateDependentDialog<QnCustomFileDialog> (this, tr("Select file..."), qnSettings->mediaFolder(), supportedFormats));
    dialog->setFileMode(QFileDialog::ExistingFile);

    int cropSoundSecs = 5;
    QString title;

    dialog->addSpinBox(tr("Clip sound up to %1 seconds").arg(QnCustomFileDialog::valueSpacer()), 1, 10, &cropSoundSecs);
    dialog->addLineEdit(tr("Custom Title:"), &title); // TODO: #Elric #TR no caps in 2nd word
    if(!dialog->exec())
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open. */
    if (!context()->user())
        return;

    QString fileName = dialog->selectedFile();
    if (fileName.isEmpty())
        return;

    if (!context()->instance<QnAppServerNotificationCache>()->storeSound(fileName, cropSoundSecs*1000, title))
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("File cannot be added."));
}

void QnNotificationSoundManagerDialog::at_renameButton_clicked() {
    if (!ui->listView->currentIndex().isValid())
        return;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    QString filename = soundModel->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString title = soundModel->titleByFilename(filename);

    QString newTitle = QInputDialog::getText(this,
                                             tr("Rename sound"),
                                             tr("Enter new title:"),
                                             QLineEdit::Normal,
                                             title);
    if (newTitle.isEmpty())
        return;

    if (!context()->instance<QnAppServerNotificationCache>()->updateTitle(filename, newTitle))
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("New title could not be set."));

}

void QnNotificationSoundManagerDialog::at_deleteButton_clicked() {
    //TODO: #GDM #Common progressbar required
    if (!ui->listView->currentIndex().isValid())
        return;

    QnNotificationSoundModel* soundModel = context()->instance<QnAppServerNotificationCache>()->persistentGuiModel();
    QString filename = soundModel->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString title = soundModel->titleByFilename(filename);
    if (QMessageBox::question(this,
                              tr("Confirm file deletion"),
                              tr("Are you sure you want to delete '%1'?").arg(title),
                              QMessageBox::Ok,
                              QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    context()->instance<QnAppServerNotificationCache>()->deleteFile(filename);
}
