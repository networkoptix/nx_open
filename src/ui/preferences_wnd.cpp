#include "preferences_wnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui_common.h"
#include "ui_licensekey.h"
#include "device/network_device.h"
#include "version.h"
#include "util.h"
#include "recordingsettingswidget.h"

extern QString button_layout;
extern QString button_home;

PreferencesWindow::PreferencesWindow(QWidget *parent) :
    QDialog(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    setupUi(this);

#ifdef CL_TRIAL_MODE
    tabWidget->removeTab(tabWidget->indexOf(licenseTab));
#endif

    creditsLabel->setText(creditsLabel->text().replace(QLatin1String("QT_VERSION"), QLatin1String(QT_VERSION_STR)));
    Settings::instance().fillData(m_settingsData);

    setWindowTitle(tr("Preferences Editor"));
    videoRecorderWidget = new RecordingSettingsWidget;
    tabWidget->insertTab(3, videoRecorderWidget, tr("Screen Recorder"));

    QPalette palette = restartIsNeededWarningLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::red);
    restartIsNeededWarningLabel->setPalette(palette);

    updateView();
    updateCameras();

    connect(&CLDeviceManager::instance().getDeviceSearcher(), SIGNAL(newNetworkDevices()), this, SLOT(updateCameras()));
    //setWindowOpacity(.90);

    resizeEvent(0);
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
    CLDeviceManager::instance().pleaseCheckDirs(checkLst);

    videoRecorderWidget->accept();

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
    {
        CLDeviceSearcher& searcher = CLDeviceManager::instance().getDeviceSearcher();
        QMutexLocker lock(&searcher.all_devices_mtx);

        cl_log.log("Updating camera list", cl_logALWAYS);
        CLDeviceList& devices =  searcher.getAllDevices();
        m_cameras.clear();
        foreach(CLDevice *device, devices.values())
        {
            if (!device->checkDeviceTypeFlag(CLDevice::NETWORK))
                continue;

            m_cameras.append(CameraNameAndInfo(device->getName(), cameraInfoString(device)));
        }
    }

    updateView();
}

QString PreferencesWindow::cameraInfoString(CLDevice *device)
{
    CLNetworkDevice* networkDevice = (CLNetworkDevice*)device;
    return QString::fromLatin1("Name: %1\nCamera MAC Address: %2\nCamera IP Address: %3\nLocal IP Address: %4")
            .arg(device->getName())
            .arg(networkDevice->getMAC())
            .arg(networkDevice->getIP().toString())
            .arg(networkDevice->getDiscoveryAddr().toString());
}

void PreferencesWindow::resizeEvent ( QResizeEvent * /*event*/)
{
//    QSize sz = this->size();
}

QString browseForDirectory()
{
    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (fileDialog.exec())
        return QDir::toNativeSeparators(fileDialog.selectedFiles().at(0));

    return QString();
}

void PreferencesWindow::mainMediaFolderBrowse()
{
    QString xdir = browseForDirectory();
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
    QString xdir = browseForDirectory();
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
    cameraInfoLabel->setText(m_cameras[row].second);
}

void PreferencesWindow::enterLicenseClick()
{
    QDialog* dialog = new QDialog();
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
