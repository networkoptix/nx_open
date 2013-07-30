#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include <translation/translation_list_model.h>

#include <core/resource/resource_directory_browser.h>
#include <decoders/abstractvideodecoderplugin.h>
#include <plugins/plugin_manager.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/network/nettools.h>
#include <client/client_settings.h>
#include <client/client_translation_manager.h>
#include <common/common_module.h>

#include "ui/actions/action_manager.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench_access_controller.h"
#include "ui/screen_recording/screen_recorder.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/server_settings_widget.h>
#include <ui/workbench/workbench_auto_starter.h>


QnPreferencesDialog::QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent):
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::PreferencesDialog()),
    m_recordingSettingsWidget(NULL),
    m_youTubeSettingsWidget(NULL),
    m_popupSettingsWidget(NULL),
    m_licenseManagerWidget(NULL),
    m_serverSettingsWidget(NULL),
    m_settings(qnSettings),
    m_licenseTabIndex(0),
    m_serverSettingsTabIndex(0),
    m_popupSettingsTabIndex(0)
{
    ui->setupUi(this);

    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);

    if (QnScreenRecorder::isSupported()) {
        m_recordingSettingsWidget = new QnRecordingSettingsWidget(this);
        ui->tabWidget->addTab(m_recordingSettingsWidget, tr("Screen Recorder"));
    }

    if(!context->instance<QnWorkbenchAutoStarter>()->isSupported()) {
        ui->autoStartCheckBox->hide();
        ui->autoStartLabel->hide();
    }

    m_popupSettingsWidget = new QnPopupSettingsWidget(context, this);
    m_popupSettingsTabIndex = ui->tabWidget->addTab(m_popupSettingsWidget, tr("Notifications"));

#if 0
    youTubeSettingsWidget = new YouTubeSettingsWidget(this);
    tabWidget->addTab(youTubeSettingsWidget, tr("YouTube"));
#endif

#ifndef CL_TRIAL_MODE
    m_licenseManagerWidget = new QnLicenseManagerWidget(this);
    m_licenseTabIndex = ui->tabWidget->addTab(m_licenseManagerWidget, tr("Licenses"));
#endif

    m_serverSettingsWidget = new QnServerSettingsWidget(this);
    m_serverSettingsTabIndex = ui->tabWidget->addTab(m_serverSettingsWidget, tr("Server"));
    resize(1, 1); // set widget size to minimal possible

    /* Set up context help. */
    setHelpTopic(ui->mainMediaFolderGroupBox, ui->extraMediaFoldersGroupBox,  Qn::SystemSettings_General_MediaFolders_Help);
    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       Qn::SystemSettings_General_TourCycleTime_Help);
    setHelpTopic(ui->showIpInTreeLabel,       ui->showIpInTreeCheckBox,       Qn::SystemSettings_General_ShowIpInTree_Help);
    setHelpTopic(ui->languageLabel,           ui->languageComboBox,           Qn::SystemSettings_General_Language_Help);
    setHelpTopic(ui->networkInterfacesGroupBox,                               Qn::SystemSettings_General_NetworkInterfaces_Help);
    if(m_recordingSettingsWidget)
        setHelpTopic(m_recordingSettingsWidget,                               Qn::SystemSettings_ScreenRecording_Help);
    if(m_licenseManagerWidget)
        setHelpTopic(m_licenseManagerWidget,                                  Qn::SystemSettings_Licenses_Help);

    at_onDecoderPluginsListChanged();

    connect( PluginManager::instance(), SIGNAL(pluginLoaded()), this, SLOT(at_onDecoderPluginsListChanged()) );

    connect(ui->browseMainMediaFolderButton,            SIGNAL(clicked()),                                          this,   SLOT(at_browseMainMediaFolderButton_clicked()));
    connect(ui->addExtraMediaFolderButton,              SIGNAL(clicked()),                                          this,   SLOT(at_addExtraMediaFolderButton_clicked()));
    connect(ui->removeExtraMediaFolderButton,           SIGNAL(clicked()),                                          this,   SLOT(at_removeExtraMediaFolderButton_clicked()));
    connect(ui->extraMediaFoldersList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),   this,   SLOT(at_extraMediaFoldersList_selectionChanged()));
    connect(ui->buttonBox,                              SIGNAL(accepted()),                                         this,   SLOT(accept()));
    connect(ui->buttonBox,                              SIGNAL(rejected()),                                         this,   SLOT(reject()));
    connect(context,                                    SIGNAL(userChanged(const QnUserResourcePtr &)),             this,   SLOT(at_context_userChanged()));
    connect(ui->timeModeComboBox,                       SIGNAL(activated(int)),                                     this,   SLOT(at_timeModeComboBox_activated()));
    connect(ui->clearCacheButton,                       SIGNAL(clicked()),                                          action(Qn::ClearCacheAction), SLOT(trigger()));

    initTranslations();
    updateFromSettings();
    at_context_userChanged();

    if (m_settings->isWritable()) {
        ui->readOnlyWarningLabel->hide();
    } else {
        setWarningStyle(ui->readOnlyWarningLabel);
        ui->readOnlyWarningLabel->setText(
#ifdef Q_OS_LINUX
            tr("Settings file is read-only. Please contact your system administrator.\nAll changes will be lost after program exit.")
#else
            tr("Settings cannot be saved. Please contact your system administrator.\nAll changes will be lost after program exit.")
#endif
        );
    }
}

QnPreferencesDialog::~QnPreferencesDialog() {
    return;
}

void QnPreferencesDialog::initTranslations() {
    QnTranslationListModel *model = new QnTranslationListModel(this);
    model->setTranslations(qnCommon->instance<QnClientTranslationManager>()->loadTranslations());
    ui->languageComboBox->setModel(model);
}

void QnPreferencesDialog::accept() {
    QString oldTranslationPath = m_settings->translationPath();
    
    submitToSettings();
    
    if (oldTranslationPath != m_settings->translationPath()) {
        QMessageBox::StandardButton button = QMessageBox::information(
            this, 
            tr("Information"), 
            tr("The language change will take effect after application restart. Press OK to close application now."),
            QMessageBox::Ok, 
            QMessageBox::Cancel
        );
        if (button == QMessageBox::Ok)
            qApp->exit();
    }
    
    base_type::accept();
}

void QnPreferencesDialog::submitToSettings() {
    m_settings->setMediaFolder(ui->mainMediaFolderLabel->text());
    m_settings->setAudioDownmixed(ui->downmixAudioCheckBox->isChecked());
    m_settings->setTourCycleTime(ui->tourCycleTimeSpinBox->value() * 1000);
    m_settings->setIpShownInTree(ui->showIpInTreeCheckBox->isChecked());
    m_settings->setUseHardwareDecoding(ui->hardwareDecodingCheckBox->isChecked());
    m_settings->setTimeMode(static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex()).toInt()));
    m_settings->setAutoStart(ui->autoStartCheckBox->isChecked());

    QStringList extraMediaFolders;
    for(int i = 0; i < ui->extraMediaFoldersList->count(); i++)
        extraMediaFolders.push_back(ui->extraMediaFoldersList->item(i)->text());
    m_settings->setExtraMediaFolders(extraMediaFolders);

    QStringList checkLst(m_settings->extraMediaFolders());
    checkLst.push_back(QDir::toNativeSeparators(m_settings->mediaFolder()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst); // TODO: #Elric re-check if it is needed here.

    QnTranslation translation = ui->languageComboBox->itemData(ui->languageComboBox->currentIndex(), Qn::TranslationRole).value<QnTranslation>();
    if(!translation.isEmpty()) {
        if(!translation.filePaths().isEmpty())
            m_settings->setTranslationPath(translation.filePaths()[0]);
    }

    if (m_recordingSettingsWidget)
        m_recordingSettingsWidget->submitToSettings();
    if (m_serverSettingsWidget)
        m_serverSettingsWidget->submit();
    if (m_popupSettingsWidget)
        m_popupSettingsWidget->submit();

    m_settings->save();
}

void QnPreferencesDialog::updateFromSettings() {
    ui->mainMediaFolderLabel->setText(QDir::toNativeSeparators(m_settings->mediaFolder()));
    ui->downmixAudioCheckBox->setChecked(m_settings->isAudioDownmixed());
    ui->tourCycleTimeSpinBox->setValue(m_settings->tourCycleTime() / 1000);
    ui->showIpInTreeCheckBox->setChecked(m_settings->isIpShownInTree());
    ui->hardwareDecodingCheckBox->setChecked(m_settings->isHardwareDecodingUsed());
    ui->timeModeComboBox->setCurrentIndex(ui->timeModeComboBox->findData(m_settings->timeMode()));
    ui->autoStartCheckBox->setChecked(m_settings->autoStart());

    ui->extraMediaFoldersList->clear();
    foreach (const QString &extraMediaFolder, m_settings->extraMediaFolders())
        ui->extraMediaFoldersList->addItem(QDir::toNativeSeparators(extraMediaFolder));

    ui->networkInterfacesList->clear();
    foreach (const QNetworkAddressEntry &entry, getAllIPv4AddressEntries())
        ui->networkInterfacesList->addItem(tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString()));

    if(m_recordingSettingsWidget)
        m_recordingSettingsWidget->updateFromSettings();

    QString translationPath = m_settings->translationPath();
    for(int i = 0; i < ui->languageComboBox->count(); i++) {
        QnTranslation translation = ui->languageComboBox->itemData(i, Qn::TranslationRole).value<QnTranslation>();
        if(translation.filePaths().contains(translationPath)) {
            ui->languageComboBox->setCurrentIndex(i);
            break;
        }
    }
}

void QnPreferencesDialog::openLicensesPage() {
    ui->tabWidget->setCurrentIndex(m_licenseTabIndex);
}

void QnPreferencesDialog::openServerSettingsPage() {
    ui->tabWidget->setCurrentIndex(m_serverSettingsTabIndex);
    m_serverSettingsWidget->updateFocusedElement();
}

void QnPreferencesDialog::openPopupSettingsPage() {
    ui->tabWidget->setCurrentIndex(m_popupSettingsTabIndex);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnPreferencesDialog::at_browseMainMediaFolderButton_clicked() {
    QFileDialog fileDialog(this);
    fileDialog.setDirectory(ui->mainMediaFolderLabel->text());
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (!fileDialog.exec())
        return;

    QString dir = QDir::toNativeSeparators(fileDialog.selectedFiles().first());
    if (dir.isEmpty())
        return;

    ui->mainMediaFolderLabel->setText(dir);
}

void QnPreferencesDialog::at_addExtraMediaFolderButton_clicked() {
    QFileDialog fileDialog(this);
    //TODO: #Elric call setDirectory
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (!fileDialog.exec())
        return;

    QString dir = QDir::toNativeSeparators(fileDialog.selectedFiles().first());
    if (dir.isEmpty())
        return;

    if(!ui->extraMediaFoldersList->findItems(dir, Qt::MatchExactly).empty()) {
        QMessageBox::information(this, tr("Folder is already added"), tr("This folder is already added."), QMessageBox::Ok);
        return;
    }

    ui->extraMediaFoldersList->addItem(dir);
}

void QnPreferencesDialog::at_removeExtraMediaFolderButton_clicked() {
    foreach(QListWidgetItem *item, ui->extraMediaFoldersList->selectedItems())
        delete item;
}

void QnPreferencesDialog::at_extraMediaFoldersList_selectionChanged() {
    ui->removeExtraMediaFolderButton->setEnabled(!ui->extraMediaFoldersList->selectedItems().isEmpty());
}

void QnPreferencesDialog::at_context_userChanged() {
    bool isAdmin = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;

    ui->tabWidget->setTabEnabled(m_licenseTabIndex, isAdmin);
    ui->tabWidget->setTabEnabled(m_serverSettingsTabIndex, isAdmin);
    if (isAdmin) {
        m_serverSettingsWidget->update();
    }
}

void QnPreferencesDialog::at_timeModeComboBox_activated() {
    if(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex(), Qt::UserRole).toInt() == Qn::ClientTimeMode) {
        QMessageBox::warning(this, tr("Warning"), tr("This settings will not affect Recording Schedule. \nRecording Schedule is always based on Server Time."));
    }
}

void QnPreferencesDialog::at_onDecoderPluginsListChanged()
{
    //checking, whether hardware decoding plugin present
    const QList<QnAbstractVideoDecoderPlugin*>& plugins = PluginManager::instance()->findPlugins<QnAbstractVideoDecoderPlugin>();
    foreach( QnAbstractVideoDecoderPlugin* plugin, plugins )
    {
        if( plugin->isHardwareAccelerated() )
        {
            ui->hardwareDecodingCheckBox->show();
            return;
        }
    }

    ui->hardwareDecodingCheckBox->hide();
    ui->hardwareDecodingLabel->hide();
}
