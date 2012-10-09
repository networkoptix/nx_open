#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QtCore/QDir>
#include <QtGui/QToolButton>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <core/resource/resource_directory_browser.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/network/nettools.h>
#include "utils/settings.h"

#include "ui/actions/action_manager.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench_translation_manager.h"
#include "ui/screen_recording/screen_recorder.h"

#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <youtube/youtubesettingswidget.h>

QnPreferencesDialog::QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent): 
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::PreferencesDialog()),
    m_recordingSettingsWidget(NULL), 
    m_youTubeSettingsWidget(NULL), 
    m_licenseManagerWidget(NULL),
    m_settings(qnSettings),
    m_licenseTabIndex(0)
{
    ui->setupUi(this);

    ui->maxVideoItemsLabel->hide();
    ui->maxVideoItemsSpinBox->hide(); // TODO: Cannot edit max number of items on the scene.

    ui->backgroundColorPicker->setAutoFillBackground(false);
    initColorPicker();

    if(!m_settings->isBackgroundEditable()) {
        ui->animateBackgroundLabel->hide();
        ui->animateBackgroundCheckBox->hide();
        ui->backgroundColorLabel->hide();
        ui->backgroundColorWidget->hide();
    }

    if (QnScreenRecorder::isSupported()){
        m_recordingSettingsWidget = new QnRecordingSettingsWidget(this);
        ui->tabWidget->addTab(m_recordingSettingsWidget, tr("Screen Recorder"));
    }

#if 0
    youTubeSettingsWidget = new YouTubeSettingsWidget(this);
    tabWidget->addTab(youTubeSettingsWidget, tr("YouTube"));
#endif

#ifndef CL_TRIAL_MODE
    m_licenseManagerWidget = new QnLicenseManagerWidget(this);
    m_licenseTabIndex = ui->tabWidget->addTab(m_licenseManagerWidget, tr("Licenses"));
#endif

    connect(ui->browseMainMediaFolderButton,            SIGNAL(clicked()),                                          this, SLOT(at_browseMainMediaFolderButton_clicked()));
    connect(ui->addExtraMediaFolderButton,              SIGNAL(clicked()),                                          this, SLOT(at_addExtraMediaFolderButton_clicked()));
    connect(ui->removeExtraMediaFolderButton,           SIGNAL(clicked()),                                          this, SLOT(at_removeExtraMediaFolderButton_clicked()));
    connect(ui->extraMediaFoldersList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),   this, SLOT(at_extraMediaFoldersList_selectionChanged()));
    connect(ui->animateBackgroundCheckBox,              SIGNAL(stateChanged(int)),                                  this, SLOT(at_animateBackgroundCheckBox_stateChanged(int)));
    connect(ui->backgroundColorPicker,                  SIGNAL(colorChanged(const QColor &)),                       this, SLOT(at_backgroundColorPicker_colorChanged(const QColor &)));
    connect(ui->buttonBox,                              SIGNAL(accepted()),                                         this, SLOT(accept()));
    connect(ui->buttonBox,                              SIGNAL(rejected()),                                         this, SLOT(reject()));

    initLanguages();

    updateFromSettings();
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

    //TODO: #gdm remove after release
    //ui->languageLabel->setVisible(false);
    //ui->languageComboBox->setVisible(false);
}

void QnPreferencesDialog::accept() {
    QString oldLanguage = m_settings->translationPath();
    submitToSettings();
    if (oldLanguage != m_settings->translationPath())
        QMessageBox::information(this, tr("Information"), tr("The language change will take effect after application restart."));
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

    QStringList extraMediaFolders;
    for(int i = 0; i < ui->extraMediaFoldersList->count(); i++)
        extraMediaFolders.push_back(ui->extraMediaFoldersList->item(i)->text());
    m_settings->setExtraMediaFolders(extraMediaFolders);

    if (m_recordingSettingsWidget)
        m_recordingSettingsWidget->submitToSettings();

    QStringList checkLst(m_settings->extraMediaFolders());
    checkLst.push_back(QDir::toNativeSeparators(m_settings->mediaFolder()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst); // TODO: re-check if it is needed here.

    m_settings->setLanguage(ui->languageComboBox->itemData(ui->languageComboBox->currentIndex()).toString());

    m_settings->save();
}

void QnPreferencesDialog::updateFromSettings() {
    ui->animateBackgroundCheckBox->setChecked(m_settings->isBackgroundAnimated());
    ui->backgroundColorPicker->setCurrentColor(m_settings->backgroundColor());
    ui->mainMediaFolderLabel->setText(QDir::toNativeSeparators(m_settings->mediaFolder()));
    ui->maxVideoItemsSpinBox->setValue(m_settings->maxVideoItems());
    ui->downmixAudioCheckBox->setChecked(m_settings->isAudioDownmixed());
    ui->tourCycleTimeSpinBox->setValue(m_settings->tourCycleTime() / 1000);

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

void QnPreferencesDialog::openLicensesPage()
{
    ui->tabWidget->setCurrentIndex(m_licenseTabIndex);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnPreferencesDialog::at_browseMainMediaFolderButton_clicked() {
    QFileDialog fileDialog(this);
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

void QnPreferencesDialog::at_removeExtraMediaFolderButton_clicked()
{
    foreach(QListWidgetItem *item, ui->extraMediaFoldersList->selectedItems())
        delete item;
}

void QnPreferencesDialog::at_extraMediaFoldersList_selectionChanged() {
    ui->removeExtraMediaFolderButton->setEnabled(!ui->extraMediaFoldersList->selectedItems().isEmpty());
}

void QnPreferencesDialog::at_animateBackgroundCheckBox_stateChanged(int state) 
{
    bool enabled = state == Qt::Checked;

    ui->backgroundColorLabel->setEnabled(enabled);
    ui->backgroundColorPicker->setEnabled(enabled);
}

void QnPreferencesDialog::at_backgroundColorPicker_colorChanged(const QColor &color) 
{
    if(color == Qt::black) {
        ui->backgroundColorPicker->setCurrentColor(Qt::white);
        ui->animateBackgroundCheckBox->setChecked(false);
    }
}
