#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

#include <QtCore/QFileInfo>
#include <QtWidgets/QInputDialog>

#include <client/client_settings.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/app_server_notification_cache.h>
#include <utils/media/audio_player.h>

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::QnNotificationSoundManagerDialog),
    m_model(context()->instance<QnAppServerNotificationCache>()->persistentGuiModel())
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::EventsActions_PlaySound_Help);

    ui->listView->setModel(m_model);

    connect(ui->playButton, &QPushButton::clicked, this,
        &QnNotificationSoundManagerDialog::at_playButton_clicked);
    connect(ui->addButton, &QPushButton::clicked, this,
        &QnNotificationSoundManagerDialog::at_addButton_clicked);
    connect(ui->renameButton, &QPushButton::clicked, this,
        &QnNotificationSoundManagerDialog::at_renameButton_clicked);
    connect(ui->deleteButton, &QPushButton::clicked, this,
        &QnNotificationSoundManagerDialog::at_deleteButton_clicked);
}

QnNotificationSoundManagerDialog::~QnNotificationSoundManagerDialog()
{
}

void QnNotificationSoundManagerDialog::enablePlayButton()
{
    ui->playButton->setEnabled(true);
}

void QnNotificationSoundManagerDialog::at_playButton_clicked()
{
    if (!ui->listView->currentIndex().isValid())
        return;

    QString filename = m_model->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(filename);
    if (!QFileInfo(filePath).exists())
        return;

    bool result = AudioPlayer::playFileAsync(filePath, this, SLOT(enablePlayButton()));
    if (result)
        ui->playButton->setEnabled(false);
}

void QnNotificationSoundManagerDialog::at_addButton_clicked()
{
    //TODO: #GDM #Common progressbar required

    QString supportedFormats = tr("Sound Files");
    supportedFormats += QLatin1String(" (*.wav *.mp3 *.ogg *.wma)");

    QScopedPointer<QnSessionAwareFileDialog> dialog(
        new QnSessionAwareFileDialog(
            this,
            tr("Select File..."),
            qnSettings->mediaFolder(),
            supportedFormats));
    dialog->setFileMode(QFileDialog::ExistingFile);

    int cropSoundSecs = 5;
    QString title;

    dialog->addSpinBox(tr("Clip sound up to %1 seconds")
        .arg(QnCustomFileDialog::valueSpacer()), 1, 10, &cropSoundSecs);
    dialog->addLineEdit(tr("Custom title:"), &title);
    if (!dialog->exec())
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open. */
    if (!context()->user())
        return;

    QString fileName = dialog->selectedFile();
    if (fileName.isEmpty())
        return;

    if (!context()->instance<QnAppServerNotificationCache>()->storeSound(
        fileName, cropSoundSecs * 1000, title))
    {
        QnMessageBox::_warning(this, tr("Failed to add file"));
    }
}

void QnNotificationSoundManagerDialog::at_renameButton_clicked()
{
    if (!ui->listView->currentIndex().isValid())
        return;

    QString filename = m_model->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString title = m_model->titleByFilename(filename);

    QString newTitle = QInputDialog::getText(this,
        tr("Rename sound"),
        tr("Enter New Title:"),
        QLineEdit::Normal,
        title);
    if (newTitle.isEmpty())
        return;

    if (!context()->instance<QnAppServerNotificationCache>()->updateTitle(filename, newTitle))
        QnMessageBox::_warning(this, tr("Failed to set new title"));

}

void QnNotificationSoundManagerDialog::at_deleteButton_clicked()
{
    //TODO: #GDM #Common progressbar required
    if (!ui->listView->currentIndex().isValid())
        return;

    QString filename = m_model->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString title = m_model->titleByFilename(filename);

    QnMessageBox dialog(QnMessageBoxIcon::Question, tr("Delete sound?"), title,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);
    dialog._addCustomButton(QnMessageBoxCustomButton::Delete);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    context()->instance<QnAppServerNotificationCache>()->deleteFile(filename);
}
