#include "preferencesdialog.h"

#include "ui/video_cam_layout/start_screen_content.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/ui_common.h"
#include "ui/context_menu_helper.h"

#include "connectionssettingswidget.h"
#include "licensewidget.h"
#include "recordingsettingswidget.h"
#include "youtube/youtubesettingswidget.h"

#include <core/resource/directory_browser.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <utils/common/util.h>
#include <utils/network/nettools.h>

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


PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
      connectionsSettingsWidget(0), videoRecorderWidget(0), youTubeSettingsWidget(0), licenseWidget(0)
{
    setupUi(this);

    connect(auxMediaRootsList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(auxMediaFolderSelectionChanged()));


    connectionsSettingsWidget = new ConnectionsSettingsWidget(this);
    tabWidget->insertTab(1, connectionsSettingsWidget, tr("Connections"));

    tabWidget->removeTab(2); // ###

    videoRecorderWidget = new RecordingSettingsWidget(this);
    tabWidget->insertTab(2, videoRecorderWidget, tr("Screen Recorder"));

    youTubeSettingsWidget = new YouTubeSettingsWidget(this);
    tabWidget->insertTab(3, youTubeSettingsWidget, tr("YouTube"));

#ifndef CL_TRIAL_MODE
    licenseWidget = new LicenseWidget(this);
    tabWidget->insertTab(4, licenseWidget, tr("License"));
#endif

    QPushButton *aboutButton = buttonBox->addButton(cm_about.text(), QDialogButtonBox::HelpRole);
    connect(aboutButton, SIGNAL(clicked()), &cm_about, SLOT(trigger()));


    Settings::instance().fillData(m_settingsData);

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
    m_settingsData.maxVideoItems = maxVideoItemsSpinBox->value();
    m_settingsData.downmixAudio = downmixAudioCheckBox->isChecked();

    Settings &settings = Settings::instance();
    settings.update(m_settingsData);
    settings.save();

    QStringList checkLst(settings.auxMediaRoots());
    checkLst.push_back(QDir::toNativeSeparators(settings.mediaRoot()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst);

    if (connectionsSettingsWidget) {
        QList<Settings::ConnectionData> connections;
        foreach (const Settings::ConnectionData &conn, connectionsSettingsWidget->connections()) {
            Settings::ConnectionData connection = conn;

            if (!connection.name.isEmpty() && connection.url.isValid())
                connections.append(connection);
        }
        Settings::setConnections(connections);
    }

    if (videoRecorderWidget)
        videoRecorderWidget->accept();

    youTubeSettingsWidget->accept();

    QDialog::accept();
}

void PreferencesDialog::updateView()
{
    mediaRootLabel->setText(QDir::toNativeSeparators(m_settingsData.mediaRoot));

    auxMediaRootsList->clear();
    foreach (const QString &auxMediaRoot, m_settingsData.auxMediaRoots)
        auxMediaRootsList->addItem(QDir::toNativeSeparators(auxMediaRoot));

    const QList<QNetworkAddressEntry> ipv4entries = getAllIPv4AddressEntries();

    networkInterfacesList->clear();
    foreach (const QNetworkAddressEntry &entry, ipv4entries)
        networkInterfacesList->addItem(tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString()));

    camerasList->clear();
    foreach (const CameraNameAndInfo &camera, m_cameras)
        camerasList->addItem(camera.first);

    if (ipv4entries.isEmpty())
        cameraStatusLabel->setText(tr("No IP addresses detected. Ensure you either have static IP or there is DHCP server in your network."));
    else if (m_cameras.isEmpty())
        cameraStatusLabel->setText(tr("No cameras detected. If you're connected to router check that it doesn't block broadcasts."));
    else
        cameraStatusLabel->setText(QString());

    totalCamerasLabel->setText(tr("Total %1 cameras detected").arg(m_cameras.size()));

    maxVideoItemsSpinBox->setValue(m_settingsData.maxVideoItems);

    downmixAudioCheckBox->setChecked(m_settingsData.downmixAudio);
}

void PreferencesDialog::updateStoredConnections()
{
    QList<Settings::ConnectionData> connections;
    foreach (const Settings::ConnectionData &conn, Settings::connections()) {
        Settings::ConnectionData connection = conn;

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
    auxRemovePushButton->setEnabled(!auxMediaRootsList->selectedItems().isEmpty());
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
        UIOKMessage(this, tr("Folder is already added"), tr("This folder is already added."));
        return;
    }

    m_settingsData.auxMediaRoots.append(xdir);

    updateView();
}

void PreferencesDialog::auxMediaFolderRemove()
{
    foreach (QListWidgetItem *item, auxMediaRootsList->selectedItems())
        m_settingsData.auxMediaRoots.removeAll(fromNativePath(item->text()));

    updateView();
}

void PreferencesDialog::cameraSelected(int row)
{
    if (row >= 0 && row < m_cameras.size())
        cameraInfoLabel->setText(m_cameras[row].second);
}

void PreferencesDialog::setCurrentPage(PreferencesDialog::SettingsPage page)
{
    tabWidget->setCurrentIndex(int(page));
}
