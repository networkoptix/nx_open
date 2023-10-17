// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

#include <QtCore/QFileInfo>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/input_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/models/notification_sound_model.h>
#include <ui/workbench/workbench_context.h>
#include <utils/media/audio_player.h>

using namespace nx::vms::client::desktop;

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::QnNotificationSoundManagerDialog),
    m_model(workbenchContext()->instance<ServerNotificationCache>()->persistentGuiModel())
{
    ui->setupUi(this);

    setHelpTopic(this, HelpTopic::Id::EventsActions_PlaySound);

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

    QString filePath = workbenchContext()->instance<ServerNotificationCache>()->getFullPath(filename);
    if (!QFileInfo(filePath).exists())
        return;

    bool result = AudioPlayer::playFileAsync(filePath, this, [this] { enablePlayButton(); });
    if (result)
        ui->playButton->setEnabled(false);
}

void QnNotificationSoundManagerDialog::at_addButton_clicked()
{
    // TODO: #sivanov Progressbar required.

    const QString supportedFormats = QnCustomFileDialog::createFilter(
        tr("Sound Files"),
        {"wav", "mp3", "ogg", "wma"});

    QScopedPointer<QnSessionAwareFileDialog> dialog(
        new QnSessionAwareFileDialog(
            this,
            tr("Select File..."),
            appContext()->localSettings()->mediaFolders().isEmpty()
                ? QString()
                : appContext()->localSettings()->mediaFolders().first(),
            supportedFormats));
    dialog->setFileMode(QFileDialog::ExistingFile);

    const int maxValueSec = appContext()->localSettings()->maxMp3FileDurationSec();
    int cropSoundSecs = maxValueSec / 2;
    QString title;

    dialog->addSpinBox(tr("Clip sound up to %1 seconds")
        .arg(QnCustomFileDialog::spinBoxPlaceholder()), 1, maxValueSec, &cropSoundSecs);
    dialog->addLineEdit(tr("Custom title:"), &title);
    if (!dialog->exec())
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open. */
    if (!system()->user())
        return;

    QString fileName = dialog->selectedFile();
    if (fileName.isEmpty())
        return;

    if (!workbenchContext()->instance<ServerNotificationCache>()->storeSound(
        fileName, cropSoundSecs * 1000, title))
    {
        QnMessageBox::warning(this, tr("Failed to add file"));
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

    const auto newTitle = QnInputDialog::getText(this,
        tr("Rename sound"),
        tr("Enter New Title:"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QString(),
        title);

    if (newTitle.isEmpty())
        return;

    if (!workbenchContext()->instance<ServerNotificationCache>()->updateTitle(filename, newTitle))
        QnMessageBox::warning(this, tr("Failed to set new title"));
}

void QnNotificationSoundManagerDialog::at_deleteButton_clicked()
{
    // TODO: #sivanov Progressbar required.
    if (!ui->listView->currentIndex().isValid())
        return;

    QString filename = m_model->filenameByRow(ui->listView->currentIndex().row());
    if (filename.isEmpty())
        return;

    QString title = m_model->titleByFilename(filename);

    QnMessageBox dialog(QnMessageBoxIcon::Question, tr("Delete sound?"), title,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);
    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    workbenchContext()->instance<ServerNotificationCache>()->deleteFile(filename);
}
