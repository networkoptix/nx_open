#include "preferences_wnd.h"

#include "ui/video_cam_layout/start_screen_content.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/ui_common.h"
#include "connectionssettingswidget.h"
#include "licensewidget.h"
#include "recordingsettingswidget.h"
#include "youtube/youtubesettingswidget.h"

#include "core/resource/directory_browser.h"
#include "core/resource/network_resource.h"
#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "utils/network/nettools.h"
#include "version.h"

static inline QString cameraInfoString(QnResourcePtr resource)
{
    QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (networkResource) {
        return PreferencesWindow::tr("Name: %1\nCamera MAC Address: %2\nCamera IP Address: %3\nLocal IP Address: %4")
            .arg(networkResource->getName())
            .arg(networkResource->getMAC().toString())
            .arg(networkResource->getHostAddress().toString())
            .arg(networkResource->getDiscoveryAddr().toString());
    }

    return resource->getName();
}


PreferencesWindow::PreferencesWindow(QWidget *parent)
    : QDialog(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
      connectionsSettingsWidget(0), videoRecorderWidget(0), youTubeSettingsWidget(0), licenseWidget(0)
{
    setupUi(this);

    //setWindowOpacity(.90);

    QString label = creditsLabel->text().replace(QLatin1String("QT_VERSION"), QLatin1String(QT_VERSION_STR)).replace(QLatin1String("FFMPEG_VERSION"), QLatin1String(FFMPEG_VERSION));
#ifndef Q_OS_DARWIN
    label = label.replace(QLatin1String("BESPIN_STRING"), QLatin1String("<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt; font-weight:600;\">Bespin style</span><span style=\" font-size:8pt;\"> </span><span style=\" font-size:8pt; font-weight:600;\">latest svn version</span><span style=\" font-size:8pt;\"> - Copyright (C) 2007-2010 Thomas Luebking</span></p>"));
#else
    label = label.replace(QLatin1String("BESPIN_STRING"), QLatin1String(""));
#endif
    creditsLabel->setText(label);
    Settings::instance().fillData(m_settingsData);

#ifdef Q_OS_WIN
    videoRecorderWidget = new RecordingSettingsWidget(this);
    tabWidget->insertTab(3, videoRecorderWidget, tr("Screen Recorder"));
#endif

    youTubeSettingsWidget = new YouTubeSettingsWidget(this);
    tabWidget->insertTab(5, youTubeSettingsWidget, tr("YouTube"));

#ifndef CL_TRIAL_MODE
    licenseWidget = new LicenseWidget(this);
    tabWidget->insertTab(6, licenseWidget, tr("License"));
#endif

    connectionsSettingsWidget = new ConnectionsSettingsWidget(this);
    tabWidget->insertTab(2, connectionsSettingsWidget, tr("Connections"));

    updateView();
    updateCameras();
    updateStoredConnections();

    //connect(CLDeviceSearcher::instance(), SIGNAL(newNetworkDevices()), this, SLOT(updateCameras())); todo
}

PreferencesWindow::~PreferencesWindow()
{
}

void PreferencesWindow::accept()
{
    m_settingsData.maxVideoItems = maxVideoItemsSpinBox->value();
    m_settingsData.downmixAudio = downmixAudioCheckBox->isChecked();
    m_settingsData.allowChangeIP = allowChangeIPCheckBox->isChecked();

    Settings& settings = Settings::instance();
    settings.update(m_settingsData);
    settings.save();

    QStringList checkLst(settings.auxMediaRoots());
    checkLst.push_back(QDir::toNativeSeparators(settings.mediaRoot()));
    QnResourceDirectoryBrowser::instance().setPathCheckList(checkLst);

    if (connectionsSettingsWidget) {
        QList<Settings::ConnectionData> connections;
        foreach (const ConnectionsSettingsWidget::ConnectionData &conn, connectionsSettingsWidget->connections()) {
            Settings::ConnectionData connection;
            connection.name = conn.name;
            connection.url = conn.url;

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

void PreferencesWindow::updateView()
{
    versionLabel->setText(QLatin1String(APPLICATION_VERSION) + " (" + QLatin1String(APPLICATION_REVISION) + ")");
    mediaRootLabel->setText(QDir::toNativeSeparators(m_settingsData.mediaRoot));

    auxMediaRootsList->clear();
    foreach (const QString &auxMediaRoot, m_settingsData.auxMediaRoots)
        auxMediaRootsList->addItem(QDir::toNativeSeparators(auxMediaRoot));

    allowChangeIPCheckBox->setChecked(m_settingsData.allowChangeIP);

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

void PreferencesWindow::updateStoredConnections()
{
    QList<ConnectionsSettingsWidget::ConnectionData> connections;
    foreach (const Settings::ConnectionData &conn, Settings::connections()) {
        ConnectionsSettingsWidget::ConnectionData connection;
        connection.name = conn.name;
        connection.url = conn.url;

        if (!connection.name.trimmed().isEmpty()) // the last used one
            connections.append(connection);
    }
    connectionsSettingsWidget->setConnections(connections);
}

void PreferencesWindow::updateCameras()
{
    cl_log.log("Updating camera list", cl_logALWAYS);

    m_cameras.clear();
    foreach(QnResourcePtr resource, qnResPool->getResourcesWithFlag(QnResource::live))
        m_cameras.append(CameraNameAndInfo(resource->getName(), cameraInfoString(resource)));

    updateView();
}

void PreferencesWindow::mainMediaFolderBrowse()
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

void PreferencesWindow::auxMediaFolderSelectionChanged()
{
    auxRemovePushButton->setEnabled(!auxMediaRootsList->selectedItems().isEmpty());
}

void PreferencesWindow::auxMediaFolderBrowse()
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

void PreferencesWindow::auxMediaFolderRemove()
{
    foreach (QListWidgetItem *item, auxMediaRootsList->selectedItems())
        m_settingsData.auxMediaRoots.removeAll(fromNativePath(item->text()));

    updateView();
}

void PreferencesWindow::cameraSelected(int row)
{
    if (row >= 0 && row < m_cameras.size())
        cameraInfoLabel->setText(m_cameras[row].second);
}

void PreferencesWindow::setCurrentTab(int index)
{
    tabWidget->setCurrentIndex(index);
}
