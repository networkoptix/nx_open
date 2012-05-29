#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QtCore/QDir>
#include <QtGui/QToolButton>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <core/resource/resource_directory_browser.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/network/nettools.h>

#include "ui/actions/action_manager.h"
#include "ui/workbench/workbench_context.h"

#include <ui/widgets/settings/connections_settings_widget.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <youtube/youtubesettingswidget.h>

namespace {
    QString cameraInfoString(QnResourcePtr resource)
    {
        QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
        if (networkResource) {
            return QnPreferencesDialog::tr("Name: %1\nCamera MAC Address: %2\nCamera IP Address: %3\nLocal IP Address: %4")
                .arg(networkResource->getName())
                .arg(networkResource->getMAC().toString())
                .arg(networkResource->getHostAddress().toString())
                .arg(networkResource->getDiscoveryAddr().toString());
        }

        return resource->getName();
    }

} // anonymous namespace


QnPreferencesDialog::QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent): 
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::PreferencesDialog()),
    m_connectionsSettingsWidget(NULL), 
    m_recordingSettingsWidget(NULL), 
    m_youTubeSettingsWidget(NULL), 
    m_licenseManagerWidget(NULL),
    m_settings(qnSettings)
{
    ui->setupUi(this);

    ui->tabWidget->removeTab(1); /* Hide camera settings tab. */ // TODO: is it needed?

    ui->maxVideoItemsLabel->hide();
    ui->maxVideoItemsSpinBox->hide(); // TODO: Cannot edit max number of items on the scene.

    ui->backgroundColorPicker->setAutoFillBackground(false);
    initColorPicker();

#ifndef QN_HAS_BACKGROUND_COLOR_ADJUSTMENT
    ui->lookAndFeelGroupBox->hide();
#endif

    m_connectionsSettingsWidget = new QnConnectionsSettingsWidget(this);
    ui->tabWidget->insertTab(PageConnections, m_connectionsSettingsWidget, tr("Connections"));

    m_recordingSettingsWidget = new QnRecordingSettingsWidget(this);
    ui->tabWidget->insertTab(PageRecordingSettings, m_recordingSettingsWidget, tr("Screen Recorder"));

#if 0
    youTubeSettingsWidget = new YouTubeSettingsWidget(this);
    tabWidget->insertTab(PageYouTubeSettings, youTubeSettingsWidget, tr("YouTube"));
#endif

#ifndef CL_TRIAL_MODE
    m_licenseManagerWidget = new QnLicenseManagerWidget(this);
    ui->tabWidget->insertTab(PageLicense, m_licenseManagerWidget, tr("Licenses"));
#endif

    connect(ui->browseMainMediaFolderButton,            SIGNAL(clicked()),                                          this, SLOT(at_browseMainMediaFolderButton_clicked()));
    connect(ui->addExtraMediaFolderButton,              SIGNAL(clicked()),                                          this, SLOT(at_addExtraMediaFolderButton_clicked()));
    connect(ui->removeExtraMediaFolderButton,           SIGNAL(clicked()),                                          this, SLOT(at_removeExtraMediaFolderButton_clicked()));
    connect(ui->extraMediaFoldersList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),   this, SLOT(at_extraMediaFoldersList_selectionChanged()));
    connect(ui->animateBackgroundCheckBox,              SIGNAL(stateChanged(int)),                                  this, SLOT(at_animateBackgroundCheckBox_stateChanged(int)));
    connect(ui->backgroundColorPicker,                  SIGNAL(colorChanged(const QColor &)),                       this, SLOT(at_backgroundColorPicker_colorChanged(const QColor &)));
    connect(ui->buttonBox,                              SIGNAL(accepted()),                                         this, SLOT(accept()));
    connect(ui->buttonBox,                              SIGNAL(rejected()),                                         this, SLOT(reject()));

    updateFromSettings();
    updateStoredConnections();
}

QnPreferencesDialog::~QnPreferencesDialog()
{
}

void QnPreferencesDialog::initColorPicker() 
{
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

void QnPreferencesDialog::accept()
{
    /*m_settingsData.animateBackground = ui->animateBackgroundCheckBox->isChecked();
    m_settingsData.backgroundColor = ui->backgroundColorPicker->currentColor();

    m_settingsData.maxVideoItems = ui->maxVideoItemsSpinBox->value();
    m_settingsData.downmixAudio = ui->downmixAudioCheckBox->isChecked();

    QnSettings *settings = qnSettings;
    settings->update(m_settingsData);
    settings->save();

    QStringList checkLst(settings->auxMediaRoots());
    checkLst.push_back(QDir::toNativeSeparators(settings->mediaRoot()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst);

    if (m_connectionsSettingsWidget) {
        QList<QnSettings::ConnectionData> connections;
        foreach (const QnSettings::ConnectionData &conn, m_connectionsSettingsWidget->connections()) {
            QnSettings::ConnectionData connection = conn;

            if (!connection.name.isEmpty() && connection.url.isValid())
                connections.append(connection);
        }
        settings->setConnections(connections);
    }

    if (m_recordingSettingsWidget)
        m_recordingSettingsWidget->accept();

    //m_youTubeSettingsWidget->accept();

    QDialog::accept();*/
}

void QnPreferencesDialog::commitToSettings() {
    /*m_settings->set

    m_settingsData.animateBackground = ui->animateBackgroundCheckBox->isChecked();
    m_settingsData.backgroundColor = ui->backgroundColorPicker->currentColor();

    m_settingsData.maxVideoItems = ui->maxVideoItemsSpinBox->value();
    m_settingsData.downmixAudio = ui->downmixAudioCheckBox->isChecked();

    QnSettings *settings = qnSettings;
    settings->update(m_settingsData);
    settings->save();

    QStringList checkLst(settings->auxMediaRoots());
    checkLst.push_back(QDir::toNativeSeparators(settings->mediaRoot()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst);*/

}

void QnPreferencesDialog::updateFromSettings()
{
#if 0
    ui->animateBackgroundCheckBox->setChecked(m_settings->isBackgroundAnimated());
    ui->backgroundColorPicker->setCurrentColor(m_settings->backgroundColor());
    ui->mediaRootLabel->setText(QDir::toNativeSeparators(m_settings->mediaRoot()));
    ui->maxVideoItemsSpinBox->setValue(m_settings->maxVideoItems());
    ui->downmixAudioCheckBox->setChecked(m_settings->downmixAudio());

    ui->auxMediaRootsList->clear();
    foreach (const QString &auxMediaRoot, m_settings->auxMediaRoots())
        ui->auxMediaRootsList->addItem(QDir::toNativeSeparators(auxMediaRoot));

    ui->networkInterfacesList->clear();
    foreach (const QNetworkAddressEntry &entry, getAllIPv4AddressEntries())
        ui->networkInterfacesList->addItem(tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString()));

    updateStoredConnections();
#endif
}

void QnPreferencesDialog::updateStoredConnections()
{
#if 0
    QList<QnSettings::ConnectionData> connections;
    foreach (const QnSettings::ConnectionData &connection, m_settings->connections())
        if (!connection.name.trimmed().isEmpty()) /* The last used one. */
            connections.append(connection);
    m_connectionsSettingsWidget->setConnections(connections);
#endif
}

void QnPreferencesDialog::at_browseMainMediaFolderButton_clicked()
{
#if 0
    QFileDialog fileDialog(this);
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (!fileDialog.exec())
        return;

    QString xdir = QDir::toNativeSeparators(fileDialog.selectedFiles().first());
    if (xdir.isEmpty())
        return;

    m_settingsData.mediaRoot = fromNativePath(xdir);

    updateFromSettings();
#endif
}

void QnPreferencesDialog::at_extraMediaFoldersList_selectionChanged()
{
    //ui->auxRemovePushButton->setEnabled(!ui->auxMediaRootsList->selectedItems().isEmpty());
}

void QnPreferencesDialog::at_addExtraMediaFolderButton_clicked()
{/*
    QFileDialog fileDialog(this);
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (!fileDialog.exec())
        return;

    QString xdir = QDir::toNativeSeparators(fileDialog.selectedFiles().first());
    if (xdir.isEmpty())
        return;

    xdir = fromNativePath(xdir);
    if (m_settingsData.auxMediaRoots.contains(xdir) || xdir == m_settingsData.mediaRoot)
    {
        QMessageBox::information(this, tr("Folder is already added"), tr("This folder is already added."), QMessageBox::Ok);
        return;
    }

    m_settingsData.auxMediaRoots.append(xdir);

    updateFromSettings();*/
}

void QnPreferencesDialog::at_removeExtraMediaFolderButton_clicked()
{
    /*foreach (QListWidgetItem *item, ui->auxMediaRootsList->selectedItems())
        m_settingsData.auxMediaRoots.removeAll(fromNativePath(item->text()));

    updateFromSettings();*/
}

void QnPreferencesDialog::setCurrentPage(QnPreferencesDialog::SettingsPage page)
{
    ui->tabWidget->setCurrentIndex(int(page));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
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
