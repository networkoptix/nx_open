#include "preferences_wnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui_common.h"
#include "ui_licensekey.h"
#include "version.h"
#include "recordingsettingswidget.h"
#include "youtube/youtubesettingswidget.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/directory_browser.h"
#include "utils/network/nettools.h"
#include "core/resource/network_resource.h"
#include "utils/common/util.h"



PreferencesWindow::PreferencesWindow(QWidget *parent) :
    QDialog(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    setupUi(this);

#ifdef CL_TRIAL_MODE
    tabWidget->removeTab(tabWidget->indexOf(licenseTab));
#endif

    QString label = creditsLabel->text().replace(QLatin1String("QT_VERSION"), QLatin1String(QT_VERSION_STR)).replace(QLatin1String("FFMPEG_VERSION"), QLatin1String(FFMPEG_VERSION));
#ifndef Q_OS_DARWIN
    label = label.replace(QLatin1String("BESPIN_STRING"), QLatin1String("<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt; font-weight:600;\">Bespin style</span><span style=\" font-size:8pt;\"> </span><span style=\" font-size:8pt; font-weight:600;\">latest svn version</span><span style=\" font-size:8pt;\"> - Copyright (C) 2007-2010 Thomas Luebking</span></p>"));
#else
    label = label.replace(QLatin1String("BESPIN_STRING"), QLatin1String(""));
#endif
    creditsLabel->setText(label);
    Settings::instance().fillData(m_settingsData);

    videoRecorderWidget = new RecordingSettingsWidget;
    tabWidget->insertTab(3, videoRecorderWidget, tr("Screen Recorder"));

    youTubeSettingsWidget = new YouTubeSettingsWidget;
    tabWidget->insertTab(5, youTubeSettingsWidget, tr("YouTube"));

    updateView();
    updateCameras();

    //connect(CLDeviceSearcher::instance(), SIGNAL(newNetworkDevices()), this, SLOT(updateCameras())); todo
    //setWindowOpacity(.90);

    resizeEvent(0);

    setWindowTitle(tr("Preferences Editor"));
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

    videoRecorderWidget->accept();
    youTubeSettingsWidget->accept();

    QDialog::accept();
}

void PreferencesWindow::updateView()
{
    versionLabel->setText(QLatin1String(APPLICATION_VERSION) + " (" + QLatin1String(APPLICATION_REVISION) + ")");
    mediaRootLabel->setText(QDir::toNativeSeparators(m_settingsData.mediaRoot));

    auxMediaRootsList->clear();
    foreach (const QString& auxMediaRoot, m_settingsData.auxMediaRoots)
    {
        auxMediaRootsList->addItem(QDir::toNativeSeparators(auxMediaRoot));
    }

    allowChangeIPCheckBox->setChecked(m_settingsData.allowChangeIP);

    if (Settings::instance().haveValidSerialNumber())
    {
        QPalette palette = licenseInfoLabel->palette();
        palette.setColor(QPalette::Foreground, Qt::black);
        licenseInfoLabel->setPalette(palette);

        licenseInfoLabel->setText(tr("You have valid license installed"));

        licenseButton->setEnabled(false);
    }
    else
    {
        QPalette palette = licenseInfoLabel->palette();
        palette.setColor(QPalette::Foreground, Qt::red);
        licenseInfoLabel->setPalette(palette);

        licenseInfoLabel->setText(tr("You do not have valid license installed"));
        licenseButton->setEnabled(true);
    }

    QList<QNetworkAddressEntry> ipv4entries = getAllIPv4AddressEntries();

    networkInterfacesList->clear();
    foreach (QNetworkAddressEntry entry, ipv4entries)
    {
        QString entryString = tr("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString());
        networkInterfacesList->addItem(entryString);
    }

    camerasList->clear();
    foreach(CameraNameAndInfo camera, m_cameras)
    {
        camerasList->addItem(camera.first);
    }

    if (ipv4entries.size() == 0)
        cameraStatusLabel->setText(tr("No IP addresses detected. Ensure you either have static IP or there is DHCP server in your network."));
    if (ipv4entries.size() > 0 && m_cameras.size() == 0)
        cameraStatusLabel->setText(tr("No cameras detected. If you're connected to router check that it doesn't block broadcasts."));
    else
        cameraStatusLabel->setText(QString());

    totalCamerasLabel->setText(QString::fromLatin1("Total %1 cameras detected").arg(m_cameras.size()));

    maxVideoItemsSpinBox->setValue(m_settingsData.maxVideoItems);

    downmixAudioCheckBox->setChecked(m_settingsData.downmixAudio);
}

void PreferencesWindow::updateCameras()
{
    cl_log.log("Updating camera list", cl_logALWAYS);

    m_cameras.clear();
    QnResourceList rlist = qnResPool->getResourcesWithFlag(QnResource::live);
    foreach(QnResourcePtr res, rlist)
    {
        m_cameras.append(CameraNameAndInfo(res->getName(), cameraInfoString(res)));
    }

    updateView();
}

QString PreferencesWindow::cameraInfoString(QnResourcePtr device)
{
    QnNetworkResourcePtr networkDevice = qSharedPointerDynamicCast<QnNetworkResource> (device);
    if (networkDevice)
        return QString::fromLatin1("Name: %1\nCamera MAC Address: %2\nCamera IP Address: %3\nLocal IP Address: %4")
                .arg(device->getName())
                .arg(networkDevice->getMAC().toString())
                .arg(networkDevice->getHostAddress().toString())
                .arg(networkDevice->getDiscoveryAddr().toString());
    else
        return device->getName();
}

void PreferencesWindow::resizeEvent ( QResizeEvent * /*event*/)
{
//    QSize sz = this->size();
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
    foreach(QListWidgetItem* item, auxMediaRootsList->selectedItems())
    {
        m_settingsData.auxMediaRoots.removeAll(fromNativePath(item->text()));
    }

    updateView();
}

void PreferencesWindow::cameraSelected(int row)
{
    if (row < 0 || row >= m_cameras.size())
        return;

    cameraInfoLabel->setText(m_cameras[row].second);
}

void PreferencesWindow::enterLicenseClick()
{
    QDialog* dialog = new QDialog(this);
    Ui::LicenseKeyDialog uiDialog;
    uiDialog.setupUi(dialog);

    if (dialog->exec() == QDialog::Accepted)
    {
        // Set directly to global settings
        Settings::instance().setSerialNumber(uiDialog.licenseKeyEdit->text());
        updateView();
    }

    delete dialog;

}

void PreferencesWindow::setCurrentTab(int index)
{
    tabWidget->setCurrentIndex(index);
}
