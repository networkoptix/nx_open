#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

#include <QtCore/QFileInfo>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>

#include <ui/dialogs/custom_file_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>

#include <utils/app_server_notification_cache.h>
#include <utils/media/audio_player.h>

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent) :
    QDialog(parent),
    QnWorkbenchContextAware(parent),
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
    //TODO: #GDM progressbar required
    QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(this, tr("Select file...")));
    dialog->setFileMode(QFileDialog::ExistingFile);

    QString supportedFormats = tr("Sound files");
    supportedFormats += QLatin1String(" (*.wav *.mp3 *.ogg *.wma)");
    dialog->setNameFilter(supportedFormats);

    int cropSoundSecs = 5;
    QString title;

    dialog->addSpinBox(tr("Clip sound up to %1 seconds").arg(QnCustomFileDialog::valueSpacer()), 1, 10, &cropSoundSecs);
    dialog->addLineEdit(tr("Custom Title"), &title);
    if(!dialog->exec())
        return;

    QStringList files = dialog->selectedFiles();
    if (files.size() < 0)
        return;

    QString filename = files[0];

    if (!context()->instance<QnAppServerNotificationCache>()->storeSound(filename, cropSoundSecs*1000, title))
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
    //TODO: #GDM progressbar required
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
