#include "general_preferences_widget.h"
#include "ui_general_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
//#include <QtGui/QDesktopServices>

#include <client/client_settings.h>
//#include <client/client_globals.h>

#include <common/common_module.h>

//#include <core/resource/resource_directory_browser.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
//#include <ui/style/custom_style.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workaround/widgets_signals_workaround.h>
//#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_auto_starter.h>

namespace {
const int kMsecsPerMinute = 60 * 1000;
}

QnGeneralPreferencesWidget::QnGeneralPreferencesWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::GeneralPreferencesWidget)
{
    ui->setupUi(this);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    if (!QnWorkbenchAutoStarter::isSupported())
        ui->autoStartCheckBox->hide();

    setHelpTopic(ui->mainMediaFolderGroupBox, ui->extraMediaFoldersGroupBox,
        Qn::SystemSettings_General_MediaFolders_Help);
    setHelpTopic(ui->pauseOnInactivityCheckBox, Qn::SystemSettings_General_AutoPause_Help);
    setHelpTopic(ui->idleTimeoutSpinBox, ui->idleTimeoutWidget,
        Qn::SystemSettings_General_AutoPause_Help);
    setHelpTopic(ui->autoStartCheckBox, Qn::SystemSettings_General_AutoStartWithSystem_Help);

    ui->idleTimeoutWidget->setEnabled(false);

    connect(ui->browseMainMediaFolderButton, &QPushButton::clicked, this,
        &QnGeneralPreferencesWidget::at_browseMainMediaFolderButton_clicked);
    connect(ui->addExtraMediaFolderButton, &QPushButton::clicked, this,
        &QnGeneralPreferencesWidget::at_addExtraMediaFolderButton_clicked);
    connect(ui->removeExtraMediaFolderButton, &QPushButton::clicked, this,
        &QnGeneralPreferencesWidget::at_removeExtraMediaFolderButton_clicked);
    connect(ui->extraMediaFoldersList->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &QnGeneralPreferencesWidget::at_extraMediaFoldersList_selectionChanged);

    connect(ui->pauseOnInactivityCheckBox, &QCheckBox::toggled, ui->idleTimeoutWidget, &QWidget::setEnabled);
    connect(ui->pauseOnInactivityCheckBox, &QCheckBox::toggled, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->idleTimeoutSpinBox, QnSpinboxIntValueChanged,   this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->autoStartCheckBox, &QCheckBox::toggled,         this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

QnGeneralPreferencesWidget::~QnGeneralPreferencesWidget()
{
}

void QnGeneralPreferencesWidget::applyChanges()
{
    qnSettings->setMediaFolder(mainMediaFolder());
    qnSettings->setExtraMediaFolders(extraMediaFolders());
    qnSettings->setUserIdleTimeoutMSecs(userIdleTimeoutMs());
    qnSettings->setAutoStart(autoStart());
}

void QnGeneralPreferencesWidget::loadDataToUi()
{
    setMainMediaFolder(qnSettings->mediaFolder());
    setExtraMediaFolders(qnSettings->extraMediaFolders());
    setUserIdleTimeoutMs(qnSettings->userIdleTimeoutMSecs());
    setAutoStart(qnSettings->autoStart());
}

bool QnGeneralPreferencesWidget::hasChanges() const
{
    if (qnSettings->mediaFolder() != mainMediaFolder())
        return true;

    if (qnSettings->extraMediaFolders() != extraMediaFolders())
        return true;

    if (qnSettings->userIdleTimeoutMSecs() != userIdleTimeoutMs())
        return true;

    if (!QnWorkbenchAutoStarter::isSupported())
    {
        if (qnSettings->autoStart() != autoStart())
            return true;
    }

    return false;
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnGeneralPreferencesWidget::at_browseMainMediaFolderButton_clicked()
{
    QString dirName = QnFileDialog::getExistingDirectory(this,
        tr("Select folder..."),
        ui->mainMediaFolderLabel->text(),
        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;

    setMainMediaFolder(dirName);
    emit hasChangesChanged();
}

void QnGeneralPreferencesWidget::at_addExtraMediaFolderButton_clicked()
{
    QString initialDir = ui->extraMediaFoldersList->count() == 0
        ? ui->mainMediaFolderLabel->text()
        : ui->extraMediaFoldersList->item(0)->text();
    QString dirName = QnFileDialog::getExistingDirectory(this,
        tr("Select folder..."),
        initialDir,
        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;
    if (extraMediaFolders().contains(dirName))
    {
        QnMessageBox::information(this, tr("Folder has already been added."), tr("This folder has already been added."), QDialogButtonBox::Ok);
        return;
    }

    ui->extraMediaFoldersList->addItem(dirName);
    emit hasChangesChanged();
}

void QnGeneralPreferencesWidget::at_removeExtraMediaFolderButton_clicked()
{
    for (auto *item : ui->extraMediaFoldersList->selectedItems())
        delete item;
    emit hasChangesChanged();
}

void QnGeneralPreferencesWidget::at_extraMediaFoldersList_selectionChanged()
{
    ui->removeExtraMediaFolderButton->setEnabled(!ui->extraMediaFoldersList->selectedItems().isEmpty());
}

QString QnGeneralPreferencesWidget::mainMediaFolder() const
{
    return ui->mainMediaFolderLabel->text();
}

void QnGeneralPreferencesWidget::setMainMediaFolder(const QString& value)
{
    ui->mainMediaFolderLabel->setText(QDir::toNativeSeparators(value));
}

QStringList QnGeneralPreferencesWidget::extraMediaFolders() const
{
    QStringList result;
    for (int i = 0; i < ui->extraMediaFoldersList->count(); ++i)
        result << ui->extraMediaFoldersList->item(i)->text();
    return result;
}

void QnGeneralPreferencesWidget::setExtraMediaFolders(const QStringList& value)
{
    ui->extraMediaFoldersList->clear();
    for (const auto &item : value)
        ui->extraMediaFoldersList->addItem(QDir::toNativeSeparators(item));
}

quint64 QnGeneralPreferencesWidget::userIdleTimeoutMs() const
{
    return ui->pauseOnInactivityCheckBox->isChecked()
        ? ui->idleTimeoutSpinBox->value() * kMsecsPerMinute
        : 0;
}

void QnGeneralPreferencesWidget::setUserIdleTimeoutMs(quint64 value)
{
    ui->pauseOnInactivityCheckBox->setChecked(value > 0);
    if (value > 0)
        ui->idleTimeoutSpinBox->setValue(value / kMsecsPerMinute); // convert to minutes
}

bool QnGeneralPreferencesWidget::autoStart() const
{
    return ui->autoStartCheckBox->isChecked();
}

void QnGeneralPreferencesWidget::setAutoStart(bool value)
{
    ui->autoStartCheckBox->setChecked(value);
}
