#include "general_preferences_widget.h"
#include "ui_general_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include <client/client_settings.h>
#include <client/client_globals.h>

#include <common/common_module.h>

#include <core/resource/resource_directory_browser.h>

#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_auto_starter.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/local_file_cache.h>

QnGeneralPreferencesWidget::QnGeneralPreferencesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralPreferencesWidget),
    m_oldDownmix(false),
    m_oldDoubleBuffering(false)
{
    ui->setupUi(this);

    if(!this->context()->instance<QnWorkbenchAutoStarter>()->isSupported()) {
        ui->autoStartCheckBox->hide();
        ui->autoStartLabel->hide();
    }

    setHelpTopic(ui->mainMediaFolderGroupBox, ui->extraMediaFoldersGroupBox,  Qn::SystemSettings_General_MediaFolders_Help);
    setHelpTopic(ui->browseLogsButton,                                        Qn::SystemSettings_General_Logs_Help);
    setHelpTopic(ui->pauseOnInactivityLabel,  ui->pauseOnInactivityCheckBox,  Qn::SystemSettings_General_AutoPause_Help);
    setHelpTopic(ui->idleTimeoutSpinBox,      ui->idleTimeoutWidget,          Qn::SystemSettings_General_AutoPause_Help);

    setWarningStyle(ui->downmixWarningLabel);
    setWarningStyle(ui->doubleBufferWarningLabel);
    setWarningStyle(ui->doubleBufferRestartLabel);
    ui->downmixWarningLabel->setVisible(false);
    ui->idleTimeoutWidget->setEnabled(false);
    ui->doubleBufferRestartLabel->setVisible(false);
    ui->doubleBufferWarningLabel->setVisible(false);

    connect(ui->browseMainMediaFolderButton,            &QPushButton::clicked,          this,   &QnGeneralPreferencesWidget::at_browseMainMediaFolderButton_clicked);
    connect(ui->addExtraMediaFolderButton,              &QPushButton::clicked,          this,   &QnGeneralPreferencesWidget::at_addExtraMediaFolderButton_clicked);
    connect(ui->removeExtraMediaFolderButton,           &QPushButton::clicked,          this,   &QnGeneralPreferencesWidget::at_removeExtraMediaFolderButton_clicked);
    connect(ui->extraMediaFoldersList->selectionModel(),&QItemSelectionModel::selectionChanged,                                         
                                                                                        this,   &QnGeneralPreferencesWidget::at_extraMediaFoldersList_selectionChanged);
    connect(ui->browseLogsButton,                       &QPushButton::clicked,          this,   &QnGeneralPreferencesWidget::at_browseLogsButton_clicked);
    connect(ui->clearCacheButton,                       &QPushButton::clicked,          this,   &QnGeneralPreferencesWidget::at_clearCacheButton_clicked);
    connect(ui->pauseOnInactivityCheckBox,              &QCheckBox::toggled,            ui->idleTimeoutWidget,          &QWidget::setEnabled);
    connect(ui->downmixAudioCheckBox,                   &QCheckBox::toggled,            this,   [this](bool toggled) {
        ui->downmixWarningLabel->setVisible(m_oldDownmix != toggled);
    });
    connect(ui->doubleBufferCheckbox,                   &QCheckBox::toggled,            this,   [this](bool toggled) {
        /* Show warning message if the user disables double buffering. */
        ui->doubleBufferWarningLabel->setVisible(!toggled && m_oldDoubleBuffering);
        /* Show restart notification if user changes value. */
        ui->doubleBufferRestartLabel->setVisible(toggled != m_oldDoubleBuffering);
    });
}

QnGeneralPreferencesWidget::~QnGeneralPreferencesWidget()
{
}

void QnGeneralPreferencesWidget::submitToSettings() {
    qnSettings->setMediaFolder(ui->mainMediaFolderLabel->text());

    QStringList extraMediaFolders;
    for(int i = 0; i < ui->extraMediaFoldersList->count(); i++)
        extraMediaFolders.push_back(ui->extraMediaFoldersList->item(i)->text());
    qnSettings->setExtraMediaFolders(extraMediaFolders);

    QStringList checkLst(qnSettings->extraMediaFolders());
    checkLst.push_back(QDir::toNativeSeparators(qnSettings->mediaFolder()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst); // TODO: #Elric re-check if it is needed here.

    qnSettings->setAudioDownmixed(ui->downmixAudioCheckBox->isChecked());
    qnSettings->setUserIdleTimeoutMSecs(ui->pauseOnInactivityCheckBox->isChecked() ? ui->idleTimeoutSpinBox->value() * 60 * 1000 : 0);
    qnSettings->setAutoStart(ui->autoStartCheckBox->isChecked());
    qnSettings->setGLDoubleBuffer(ui->doubleBufferCheckbox->isChecked());
}

void QnGeneralPreferencesWidget::updateFromSettings() {
    ui->mainMediaFolderLabel->setText(QDir::toNativeSeparators(qnSettings->mediaFolder()));

    ui->extraMediaFoldersList->clear();
    foreach (const QString &extraMediaFolder, qnSettings->extraMediaFolders())
        ui->extraMediaFoldersList->addItem(QDir::toNativeSeparators(extraMediaFolder));

    m_oldDownmix = qnSettings->isAudioDownmixed();
    ui->downmixAudioCheckBox->setChecked(m_oldDownmix);

    ui->pauseOnInactivityCheckBox->setChecked(qnSettings->userIdleTimeoutMSecs() > 0);
    if (qnSettings->userIdleTimeoutMSecs() > 0)
        ui->idleTimeoutSpinBox->setValue(qnSettings->userIdleTimeoutMSecs() / (60 * 1000)); // convert to minutes

    ui->autoStartCheckBox->setChecked(qnSettings->autoStart());

    m_oldDoubleBuffering = qnSettings->isGlDoubleBuffer();
    ui->doubleBufferCheckbox->setChecked(m_oldDoubleBuffering);
}

bool QnGeneralPreferencesWidget::confirm() {
    return m_oldDownmix == ui->downmixAudioCheckBox->isChecked()
        && m_oldDoubleBuffering == ui->doubleBufferCheckbox->isChecked();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnGeneralPreferencesWidget::at_browseMainMediaFolderButton_clicked() {
    QString dirName = QnFileDialog::getExistingDirectory(this,
                                                        tr("Select folder..."),
                                                        ui->mainMediaFolderLabel->text(),
                                                        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;
    ui->mainMediaFolderLabel->setText(dirName);
}

void QnGeneralPreferencesWidget::at_addExtraMediaFolderButton_clicked() {
    QString initialDir = ui->extraMediaFoldersList->count() == 0
            ? ui->mainMediaFolderLabel->text()
            : ui->extraMediaFoldersList->item(0)->text();
    QString dirName = QnFileDialog::getExistingDirectory(this,
                                                        tr("Select folder..."),
                                                        initialDir,
                                                        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;

    if(!ui->extraMediaFoldersList->findItems(dirName, Qt::MatchExactly).empty()) {
        QMessageBox::information(this, tr("Folder is already added"), tr("This folder is already added."), QMessageBox::Ok);
        return;
    }

    ui->extraMediaFoldersList->addItem(dirName);
}

void QnGeneralPreferencesWidget::at_removeExtraMediaFolderButton_clicked() {
    foreach(QListWidgetItem *item, ui->extraMediaFoldersList->selectedItems())
        delete item;
}

void QnGeneralPreferencesWidget::at_extraMediaFoldersList_selectionChanged() {
    ui->removeExtraMediaFolderButton->setEnabled(!ui->extraMediaFoldersList->selectedItems().isEmpty());
}

void QnGeneralPreferencesWidget::at_browseLogsButton_clicked() {
    const QString logsLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/log");
    if (!QDir(logsLocation).exists()) {
        QMessageBox::information(this,
                                 tr("Information"),
                                 tr("Folder '%1' does not exist.").arg(logsLocation));
        return;
    }
    QDesktopServices::openUrl(QLatin1String("file:///") + logsLocation);
}

void QnGeneralPreferencesWidget::at_clearCacheButton_clicked() {
    QString backgroundImage = qnSettings->background().imageName;
    /* Lock background image so it will not be deleted. */
    if (!backgroundImage.isEmpty()) {
        QnLocalFileCache cache;
        QString path = cache.getFullPath(backgroundImage);
        QFile lock(path);
        lock.open(QFile::ReadWrite);
        QnAppServerFileCache::clearLocalCache();
        lock.close();
    } else {
        QnAppServerFileCache::clearLocalCache();
    }
}