#include "preferencesdialog.h"
#include "ui_preferences.h"

#include <QDir>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>

#include <core/resource/resource_directory_browser.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/network/nettools.h>

#include "ui/actions/action_manager.h"
#include "ui/workbench/workbench_context.h"

#include "connectionssettingswidget.h"
#include "licensemanagerwidget.h"
#include "licensewidget.h"
#include "recordingsettingswidget.h"
#include "youtube/youtubesettingswidget.h"

static inline QString cameraInfoString(QnResourcePtr resource)
{
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (networkResource) {
        return PreferencesDialog::tr("Name: %1\nCamera MAC Address: %2\nCamera IP Address: %3\nLocal IP Address: %4")
            .arg(networkResource->getName())
            .arg(networkResource->getMAC().toString())
            .arg(networkResource->getHostAddress().toString())
            .arg(networkResource->getDiscoveryAddr().toString());
    }

    return resource->getName();
}


PreferencesDialog::PreferencesDialog(QnWorkbenchContext *context, QWidget *parent): 
    QDialog(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    ui(new Ui::PreferencesDialog()),
    connectionsSettingsWidget(0), 
    videoRecorderWidget(0), 
    youTubeSettingsWidget(0), 
    licenseManagerWidget(0)
{
    if(!context)
        qnNullWarning(context);

    ui->setupUi(this);

    ui->lookAndFeelGroupBox->hide(); // TODO: Cannot edit max number of items on the scene.

    connect(ui->auxMediaRootsList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(auxMediaFolderSelectionChanged()));


    connectionsSettingsWidget = new ConnectionsSettingsWidget(this);
    ui->tabWidget->insertTab(1, connectionsSettingsWidget, tr("Connections"));

    ui->tabWidget->removeTab(2); // ###

    videoRecorderWidget = new RecordingSettingsWidget(this);
    ui->tabWidget->insertTab(2, videoRecorderWidget, tr("Screen Recorder"));

#if 0
    youTubeSettingsWidget = new YouTubeSettingsWidget(this);
    tabWidget->insertTab(3, youTubeSettingsWidget, tr("YouTube"));
#endif

#ifndef CL_TRIAL_MODE
    licenseManagerWidget = new LicenseManagerWidget(this);
    ui->tabWidget->insertTab(4, licenseManagerWidget, tr("Licenses"));
#endif

    QToolButton *aboutButton = new QToolButton();
    aboutButton->setDefaultAction(context ? context->action(Qn::AboutAction) : NULL);
    aboutButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->buttonBox->addButton(aboutButton, QDialogButtonBox::HelpRole);

    qnSettings->fillData(m_settingsData);

    updateView();
    updateCameras();
    updateStoredConnections();

    //connect(CLDeviceSearcher::instance(), SIGNAL(newNetworkDevices()), this, SLOT(updateCameras())); todo
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::accept()
{
    m_settingsData.maxVideoItems = ui->maxVideoItemsSpinBox->value();
    m_settingsData.downmixAudio = ui->downmixAudioCheckBox->isChecked();

    QnSettings *settings = qnSettings;
    settings->update(m_settingsData);
    settings->save();

    QStringList checkLst(settings->auxMediaRoots());
    checkLst.push_back(QDir::toNativeSeparators(settings->mediaRoot()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst);

    if (connectionsSettingsWidget) {
        QList<QnSettings::ConnectionData> connections;
        foreach (const QnSettings::ConnectionData &conn, connectionsSettingsWidget->connections()) {
            QnSettings::ConnectionData connection = conn;

            if (!connection.name.isEmpty() && connection.url.isValid())
                connections.append(connection);
        }
        settings->setConnections(connections);
    }

    if (videoRecorderWidget)
        videoRecorderWidget->accept();

    //youTubeSettingsWidget->accept();

    QDialog::accept();
}

void PreferencesDialog::updateView()
{
    ui->mediaRootLabel->setText(QDir::toNativeSeparators(m_settingsData.mediaRoot));

    ui->auxMediaRootsList->clear();
    foreach (const QString &auxMediaRoot, m_settingsData.auxMediaRoots)
        ui->auxMediaRootsList->addItem(QDir::toNativeSeparators(auxMediaRoot));

    const QList<QNetworkAddressEntry> ipv4entries = getAllIPv4AddressEntries();

    ui->networkInterfacesList->clear();
    foreach (const QNetworkAddressEntry &entry, ipv4entries)
        ui->networkInterfacesList->addItem(tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString()));

    ui->camerasList->clear();
    foreach (const CameraNameAndInfo &camera, m_cameras)
        ui->camerasList->addItem(camera.first);

    if (ipv4entries.isEmpty())
        ui->cameraStatusLabel->setText(tr("No IP addresses detected. Ensure you either have static IP or there is DHCP server in your network."));
    else if (m_cameras.isEmpty())
        ui->cameraStatusLabel->setText(tr("No cameras detected. If you're connected to router check that it doesn't block broadcasts."));
    else
        ui->cameraStatusLabel->setText(QString());

    ui->totalCamerasLabel->setText(tr("Total %1 cameras detected").arg(m_cameras.size()));

    ui->maxVideoItemsSpinBox->setValue(m_settingsData.maxVideoItems);

    ui->downmixAudioCheckBox->setChecked(m_settingsData.downmixAudio);
}

void PreferencesDialog::updateStoredConnections()
{
    QList<QnSettings::ConnectionData> connections;
    foreach (const QnSettings::ConnectionData &conn, qnSettings->connections()) {
        QnSettings::ConnectionData connection = conn;

        if (!connection.name.trimmed().isEmpty()) // the last used one
            connections.append(connection);
    }
    connectionsSettingsWidget->setConnections(connections);
}

void PreferencesDialog::updateCameras()
{
    cl_log.log("Updating camera list", cl_logALWAYS);

    m_cameras.clear();
    foreach(QnResourcePtr resource, qnResPool->getResourcesWithFlag(QnResource::live))
        m_cameras.append(CameraNameAndInfo(resource->getName(), cameraInfoString(resource)));

    updateView();
}

void PreferencesDialog::mainMediaFolderBrowse()
{
    QFileDialog fileDialog(this);
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (!fileDialog.exec())
        return;

    QString xdir = QDir::toNativeSeparators(fileDialog.selectedFiles().first());
    if (xdir.isEmpty())
        return;

    m_settingsData.mediaRoot = fromNativePath(xdir);

    updateView();
}

void PreferencesDialog::auxMediaFolderSelectionChanged()
{
    ui->auxRemovePushButton->setEnabled(!ui->auxMediaRootsList->selectedItems().isEmpty());
}

void PreferencesDialog::auxMediaFolderBrowse()
{
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

    updateView();
}

void PreferencesDialog::auxMediaFolderRemove()
{
    foreach (QListWidgetItem *item, ui->auxMediaRootsList->selectedItems())
        m_settingsData.auxMediaRoots.removeAll(fromNativePath(item->text()));

    updateView();
}

void PreferencesDialog::cameraSelected(int row)
{
    if (row >= 0 && row < m_cameras.size())
        ui->cameraInfoLabel->setText(m_cameras[row].second);
}

void PreferencesDialog::setCurrentPage(PreferencesDialog::SettingsPage page)
{
    ui->tabWidget->setCurrentIndex(int(page));
}
