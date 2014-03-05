#include "general_preferences_widget.h"
#include "ui_general_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_translation_manager.h>

#include <common/common_module.h>

#include <core/resource/resource_directory_browser.h>
#include <translation/translation_list_model.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_auto_starter.h>

#include <utils/network/nettools.h>

QnGeneralPreferencesWidget::QnGeneralPreferencesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralPreferencesWidget)
{
    ui->setupUi(this);

    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);

    ui->skinComboBox->addItem(tr("Dark"), Qn::DarkSkin);
    ui->skinComboBox->addItem(tr("Light"), Qn::LightSkin);

    if(!this->context()->instance<QnWorkbenchAutoStarter>()->isSupported()) {
        ui->autoStartCheckBox->hide();
        ui->autoStartLabel->hide();
    }

    setHelpTopic(ui->mainMediaFolderGroupBox, ui->extraMediaFoldersGroupBox,  Qn::SystemSettings_General_MediaFolders_Help);
    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       Qn::SystemSettings_General_TourCycleTime_Help);
    setHelpTopic(ui->showIpInTreeLabel,       ui->showIpInTreeCheckBox,       Qn::SystemSettings_General_ShowIpInTree_Help);
    setHelpTopic(ui->languageLabel,           ui->languageComboBox,           Qn::SystemSettings_General_Language_Help);
    setHelpTopic(ui->networkInterfacesGroupBox,                               Qn::SystemSettings_General_NetworkInterfaces_Help);

    initTranslations();

    setWarningStyle(ui->downmixWarningLabel);
    setWarningStyle(ui->languageWarningLabel);
    setWarningStyle(ui->skinWarningLabel);
    ui->languageWarningLabel->setVisible(false);
    ui->downmixWarningLabel->setVisible(false);
    ui->skinWarningLabel->setVisible(false);
    ui->idleTimeoutWidget->setEnabled(false);

    connect(ui->browseMainMediaFolderButton,            SIGNAL(clicked()),                                          this,   SLOT(at_browseMainMediaFolderButton_clicked()));
    connect(ui->addExtraMediaFolderButton,              SIGNAL(clicked()),                                          this,   SLOT(at_addExtraMediaFolderButton_clicked()));
    connect(ui->removeExtraMediaFolderButton,           SIGNAL(clicked()),                                          this,   SLOT(at_removeExtraMediaFolderButton_clicked()));
    connect(ui->extraMediaFoldersList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),   this,   SLOT(at_extraMediaFoldersList_selectionChanged()));

    connect(ui->timeModeComboBox,                       SIGNAL(activated(int)),                                     this,   SLOT(at_timeModeComboBox_activated()));
    connect(ui->clearCacheButton,                       SIGNAL(clicked()),                                          action(Qn::ClearCacheAction), SLOT(trigger()));
    connect(ui->browseLogsButton,                       SIGNAL(clicked()),                                          this,   SLOT(at_browseLogsButton_clicked()));

    connect(ui->downmixAudioCheckBox,                   SIGNAL(toggled(bool)),                                      this,   SLOT(at_downmixAudioCheckBox_toggled(bool)));
    connect(ui->languageComboBox,                       SIGNAL(currentIndexChanged(int)),                           this,   SLOT(at_languageComboBox_currentIndexChanged(int)));
    connect(ui->skinComboBox,                           SIGNAL(currentIndexChanged(int)),                           this,   SLOT(at_skinComboBox_currentIndexChanged(int)));
    connect(ui->pauseOnInactivityCheckBox,              SIGNAL(toggled(bool)),                                      ui->idleTimeoutWidget, SLOT(setEnabled(bool)));
}

QnGeneralPreferencesWidget::~QnGeneralPreferencesWidget()
{
}

void QnGeneralPreferencesWidget::submitToSettings() {
    qnSettings->setMediaFolder(ui->mainMediaFolderLabel->text());
    qnSettings->setAudioDownmixed(ui->downmixAudioCheckBox->isChecked());
    qnSettings->setTourCycleTime(ui->tourCycleTimeSpinBox->value() * 1000);
    qnSettings->setIpShownInTree(ui->showIpInTreeCheckBox->isChecked());
    qnSettings->setTimeMode(static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex()).toInt()));
    qnSettings->setAutoStart(ui->autoStartCheckBox->isChecked());
    qnSettings->setUserIdleTimeoutMSecs(ui->pauseOnInactivityCheckBox->isChecked() ? ui->idleTimeoutSpinBox->value() * 60 * 1000 : 0);
    qnSettings->setClientSkin(static_cast<Qn::ClientSkin>(ui->skinComboBox->itemData(ui->skinComboBox->currentIndex()).toInt()));

    QStringList extraMediaFolders;
    for(int i = 0; i < ui->extraMediaFoldersList->count(); i++)
        extraMediaFolders.push_back(ui->extraMediaFoldersList->item(i)->text());
    qnSettings->setExtraMediaFolders(extraMediaFolders);

    QStringList checkLst(qnSettings->extraMediaFolders());
    checkLst.push_back(QDir::toNativeSeparators(qnSettings->mediaFolder()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst); // TODO: #Elric re-check if it is needed here.

    QnTranslation translation = ui->languageComboBox->itemData(ui->languageComboBox->currentIndex(), Qn::TranslationRole).value<QnTranslation>();
    if(!translation.isEmpty()) {
        if(!translation.filePaths().isEmpty()) {
            QString currentTranslationPath = qnSettings->translationPath();
            if(!translation.filePaths().contains(currentTranslationPath))
                qnSettings->setTranslationPath(translation.filePaths()[0]);
        }
    }
}

void QnGeneralPreferencesWidget::updateFromSettings() {
    ui->mainMediaFolderLabel->setText(QDir::toNativeSeparators(qnSettings->mediaFolder()));

    m_oldDownmix = qnSettings->isAudioDownmixed();
    ui->downmixAudioCheckBox->setChecked(m_oldDownmix);

    m_oldSkin = ui->skinComboBox->findData(qnSettings->clientSkin());
    ui->skinComboBox->setCurrentIndex(m_oldSkin);

    ui->tourCycleTimeSpinBox->setValue(qnSettings->tourCycleTime() / 1000);
    ui->showIpInTreeCheckBox->setChecked(qnSettings->isIpShownInTree());

    ui->timeModeComboBox->setCurrentIndex(ui->timeModeComboBox->findData(qnSettings->timeMode()));
    ui->autoStartCheckBox->setChecked(qnSettings->autoStart());

    ui->pauseOnInactivityCheckBox->setChecked(qnSettings->userIdleTimeoutMSecs() > 0);
    if (qnSettings->userIdleTimeoutMSecs() > 0)
        ui->idleTimeoutSpinBox->setValue(qnSettings->userIdleTimeoutMSecs() / (60 * 1000)); // convert to minutes

    ui->extraMediaFoldersList->clear();
    foreach (const QString &extraMediaFolder, qnSettings->extraMediaFolders())
        ui->extraMediaFoldersList->addItem(QDir::toNativeSeparators(extraMediaFolder));

    ui->networkInterfacesList->clear();
    foreach (const QNetworkAddressEntry &entry, getAllIPv4AddressEntries())
        ui->networkInterfacesList->addItem(tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString()));

    m_oldLanguage = 0;
    QString translationPath = qnSettings->translationPath();
    for(int i = 0; i < ui->languageComboBox->count(); i++) {
        QnTranslation translation = ui->languageComboBox->itemData(i, Qn::TranslationRole).value<QnTranslation>();
        if(translation.filePaths().contains(translationPath)) {
            m_oldLanguage = i;
            break;
        }
    }
    ui->languageComboBox->setCurrentIndex(m_oldLanguage);
}

bool QnGeneralPreferencesWidget::confirm() {
    if (m_oldDownmix != ui->downmixAudioCheckBox->isChecked() ||
        m_oldLanguage != ui->languageComboBox->currentIndex()) {
        switch(QMessageBox::information(
                    this,
                    tr("Information"),
                    tr("Some changes will take effect only after application restart. Do you want to restart the application now?"),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    QMessageBox::Yes
        )) {
        case QMessageBox::Cancel:
            return false;
        case QMessageBox::Yes:
            menu()->trigger(Qn::QueueAppRestartAction);
            break;
        default:
            break;
        }
    }

    return true;
}

void QnGeneralPreferencesWidget::initTranslations() {
    QnTranslationListModel *model = new QnTranslationListModel(this);
    model->setTranslations(qnCommon->instance<QnClientTranslationManager>()->loadTranslations());
    ui->languageComboBox->setModel(model);
}

#include "ui/workaround/mac_utils.h"
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

void QnGeneralPreferencesWidget::at_timeModeComboBox_activated() {
    if(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex(), Qt::UserRole).toInt() == Qn::ClientTimeMode) {
        QMessageBox::warning(this, tr("Warning"), tr("This option will not affect Recording Schedule. \nRecording Schedule is always based on Server Time."));
    }
}

void QnGeneralPreferencesWidget::at_downmixAudioCheckBox_toggled(bool checked) {
    ui->downmixWarningLabel->setVisible(m_oldDownmix != checked);
}

void QnGeneralPreferencesWidget::at_languageComboBox_currentIndexChanged(int index) {
    ui->languageWarningLabel->setVisible(m_oldLanguage != index);
}

void QnGeneralPreferencesWidget::at_skinComboBox_currentIndexChanged(int index) {
    ui->skinWarningLabel->setVisible(m_oldSkin != index);
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
