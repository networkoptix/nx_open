#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

#include <QtCore/QFileInfo>
#include <QtGui/QFileDialog>

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
    ui->removeButton->setVisible(false);

    ui->listView->setModel(context()->instance<QnAppServerNotificationCache>()->persistentGuiModel());

    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(at_playButton_clicked()));
    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(at_addButton_clicked()));
}

QnNotificationSoundManagerDialog::~QnNotificationSoundManagerDialog()
{
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

    AudioPlayer::playFileAsync(filePath);
}

void QnNotificationSoundManagerDialog::at_addButton_clicked() {
    QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(this, tr("Select file...")));
    dialog->setFileMode(QFileDialog::ExistingFile);

    QString supportedFormats = tr("Sound files");
    supportedFormats += QLatin1String(" (*.wav *.mp3 *.ogg *.wma)");
    dialog->setNameFilter(supportedFormats);

    int cropSoundSecs = 5;

    dialog->addSpinBox(tr("Cut to first secs"), 1, 10, &cropSoundSecs);
    if(!dialog->exec())
        return;

    QStringList files = dialog->selectedFiles();
    if (files.size() < 0)
        return;

    QString filename = files[0];

    context()->instance<QnAppServerNotificationCache>()->storeSound(filename, cropSoundSecs*1000);


}
