#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QtCore/QDir>
#include <QtGui/QToolButton>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <core/resource/resource_directory_browser.h>
#include <decoders/abstractvideodecoderplugin.h>
#include <plugins/pluginmanager.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/network/nettools.h>
#include <client/client_settings.h>

#include "ui/actions/action_manager.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench_translation_manager.h"
#include "ui/workbench/workbench_access_controller.h"
#include "ui/screen_recording/screen_recorder.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/server_settings_widget.h>

#include <youtube/youtubesettingswidget.h>


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

    ui->maxVideoItemsLabel->hide();
    ui->maxVideoItemsSpinBox->hide(); // TODO: #Elric Cannot edit max number of items on the scene.

    if(m_settings->isBackgroundEditable()) {
        ui->backgroundColorPicker->setAutoFillBackground(false);
        initColorPicker();
    } else {
        ui->animateBackgroundLabel->hide();
        ui->animateBackgroundCheckBox->hide();
        ui->backgroundColorLabel->hide();
        ui->backgroundColorWidget->hide();
    }

    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);

    if (QnScreenRecorder::isSupported()) {
        m_recordingSettingsWidget = new QnRecordingSettingsWidget(this);
        ui->tabWidget->addTab(m_recordingSettingsWidget, tr("Screen Recorder"));
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
    connect( PluginManager::instance(), SIGNAL(pluginUnloaded()), this, SLOT(at_onDecoderPluginsListChanged()) );

    connect(ui->browseMainMediaFolderButton,            SIGNAL(clicked()),                                          this,   SLOT(at_browseMainMediaFolderButton_clicked()));
    connect(ui->addExtraMediaFolderButton,              SIGNAL(clicked()),                                          this,   SLOT(at_addExtraMediaFolderButton_clicked()));
    connect(ui->removeExtraMediaFolderButton,           SIGNAL(clicked()),                                          this,   SLOT(at_removeExtraMediaFolderButton_clicked()));
    connect(ui->extraMediaFoldersList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),   this,   SLOT(at_extraMediaFoldersList_selectionChanged()));
    connect(ui->animateBackgroundCheckBox,              SIGNAL(stateChanged(int)),                                  this,   SLOT(at_animateBackgroundCheckBox_stateChanged(int)));
    connect(ui->backgroundColorPicker,                  SIGNAL(colorChanged(const QColor &)),                       this,   SLOT(at_backgroundColorPicker_colorChanged(const QColor &)));
    connect(ui->buttonBox,                              SIGNAL(accepted()),                                         this,   SLOT(accept()));
    connect(ui->buttonBox,                              SIGNAL(rejected()),                                         this,   SLOT(reject()));
    connect(context,                                    SIGNAL(userChanged(const QnUserResourcePtr &)),             this,   SLOT(at_context_userChanged()));
    connect(ui->timeModeComboBox,                       SIGNAL(activated(int)),                                     this,   SLOT(at_timeModeComboBox_activated()));

    initLanguages();
    updateFromSettings();
    at_context_userChanged();

    if (m_settings->isWritable()) {
        ui->readOnlyWarningLabel->hide();
    }
    else {
        setWarningStyle(ui->readOnlyWarningLabel);
        ui->readOnlyWarningLabel->setText(
                     #ifdef Q_OS_LINUX
                             tr("Settings file is read-only. Please contact your system administrator.\n" \
                                "All changes will be lost after program exit.")
                     #else
                             tr("Settings cannot be saved. Please contact your system administrator.\n" \
                                "All changes will be lost after program exit.")
                     #endif
                             );
    }
}

QnPreferencesDialog::~QnPreferencesDialog() {
    return;
}

void QnPreferencesDialog::initColorPicker() {
    QtColorPicker *w = ui->backgroundColorPicker;

    /* No black here. */
    w->insertColor(Qt::white,         tr("White"));
    w->insertColor(Qt::red,           tr("Red"));
    w->insertColor(Qt::darkRed,       tr("Dark red"));
    w->insertColor(Qt::green,         tr("Green"));
    w->insertColor(Qt::darkGreen,     tr("Dark green"));
    w->insertColor(Qt::blue,          tr("Blue"));
    w->insertColor(Qt::darkBlue,      tr("Dark blue"));
    w->insertColor(Qt::cyan,          tr("Cyan"));
    w->insertColor(Qt::darkCyan,      tr("Dark cyan"));
    w->insertColor(Qt::magenta,       tr("Magenta"));
    w->insertColor(Qt::darkMagenta,   tr("Dark magenta"));
    w->insertColor(Qt::yellow,        tr("Yellow"));
    w->insertColor(Qt::darkYellow,    tr("Dark yellow"));
    w->insertColor(Qt::gray,          tr("Gray"));
    w->insertColor(Qt::darkGray,      tr("Dark gray"));
    w->insertColor(Qt::lightGray,     tr("Light gray"));
}

void QnPreferencesDialog::initLanguages() {
    QnWorkbenchTranslationManager *translationManager = context()->instance<QnWorkbenchTranslationManager>();

    foreach(const QnTranslationInfo &translation, translationManager->translations()){
        QIcon icon(QString(QLatin1String(":/flags/%1.png")).arg(translation.localeCode));
        ui->languageComboBox->addItem(icon, translation.languageName, translation.translationPath);
    }
}

void QnPreferencesDialog::accept() {
    QString oldLanguage = m_settings->translationPath();
    submitToSettings();
    if (oldLanguage != m_settings->translationPath()) {
        QMessageBox::StandardButton button =
            QMessageBox::information(this, tr("Information"), tr("The language change will take effect after application restart. Press OK to close application now."),
                                     QMessageBox::Ok, QMessageBox::Cancel);
        if (button == QMessageBox::Ok)
            qApp->exit();
    }
    //m_youTubeSettingsWidget->accept();
    base_type::accept();
}

void QnPreferencesDialog::submitToSettings() {
    m_settings->setBackgroundAnimated(ui->animateBackgroundCheckBox->isChecked());
    m_settings->setBackgroundColor(ui->backgroundColorPicker->currentColor());
    m_settings->setMediaFolder(ui->mainMediaFolderLabel->text());
    m_settings->setMaxVideoItems(ui->maxVideoItemsSpinBox->value());
    m_settings->setAudioDownmixed(ui->downmixAudioCheckBox->isChecked());
    m_settings->setTourCycleTime(ui->tourCycleTimeSpinBox->value() * 1000);
    m_settings->setIpShownInTree(ui->showIpInTreeCheckBox->isChecked());
    m_settings->setUseHardwareDecoding(ui->isHardwareDecodingCheckBox->isChecked());
    m_settings->setTimeMode(static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex()).toInt()));

    QStringList extraMediaFolders;
    for(int i = 0; i < ui->extraMediaFoldersList->count(); i++)
        extraMediaFolders.push_back(ui->extraMediaFoldersList->item(i)->text());
    m_settings->setExtraMediaFolders(extraMediaFolders);

    QStringList checkLst(m_settings->extraMediaFolders());
    checkLst.push_back(QDir::toNativeSeparators(m_settings->mediaFolder()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst); // TODO: #Elric re-check if it is needed here.

    m_settings->setLanguage(ui->languageComboBox->itemData(ui->languageComboBox->currentIndex()).toString());

    if (m_recordingSettingsWidget)
        m_recordingSettingsWidget->submitToSettings();
    if (m_serverSettingsWidget)
        m_serverSettingsWidget->submit();
    if (m_popupSettingsWidget)
        m_popupSettingsWidget->submit();

    m_settings->save();
}

void QnPreferencesDialog::updateFromSettings() {
    ui->animateBackgroundCheckBox->setChecked(m_settings->isBackgroundAnimated());
    ui->backgroundColorPicker->setCurrentColor(m_settings->backgroundColor());
    ui->mainMediaFolderLabel->setText(QDir::toNativeSeparators(m_settings->mediaFolder()));
    ui->maxVideoItemsSpinBox->setValue(m_settings->maxVideoItems());
    ui->downmixAudioCheckBox->setChecked(m_settings->isAudioDownmixed());
    ui->tourCycleTimeSpinBox->setValue(m_settings->tourCycleTime() / 1000);
    ui->showIpInTreeCheckBox->setChecked(m_settings->isIpShownInTree());
    ui->isHardwareDecodingCheckBox->setChecked( m_settings->isHardwareDecodingUsed() );
    ui->timeModeComboBox->setCurrentIndex(ui->timeModeComboBox->findData(m_settings->timeMode()));

    ui->extraMediaFoldersList->clear();
    foreach (const QString &extraMediaFolder, m_settings->extraMediaFolders())
        ui->extraMediaFoldersList->addItem(QDir::toNativeSeparators(extraMediaFolder));

    ui->networkInterfacesList->clear();
    foreach (const QNetworkAddressEntry &entry, getAllIPv4AddressEntries())
        ui->networkInterfacesList->addItem(tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString()));

    if(m_recordingSettingsWidget)
        m_recordingSettingsWidget->updateFromSettings();

    int id = ui->languageComboBox->findData(m_settings->translationPath());
    if (id >= 0)
        ui->languageComboBox->setCurrentIndex(id);
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

void QnPreferencesDialog::at_animateBackgroundCheckBox_stateChanged(int state) {
    bool enabled = state == Qt::Checked;

    ui->backgroundColorLabel->setEnabled(enabled);
    ui->backgroundColorPicker->setEnabled(enabled);
}

void QnPreferencesDialog::at_backgroundColorPicker_colorChanged(const QColor &color) {
    if(color == Qt::black) {
        ui->backgroundColorPicker->setCurrentColor(Qt::white);
        ui->animateBackgroundCheckBox->setChecked(false);
    }
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
            ui->isHardwareDecodingCheckBox->show();
            return;
        }
    }

    ui->isHardwareDecodingCheckBox->hide();
}
