#include "preferences_wnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui_common.h"
#include "ui_licensekey.h"
#include "device/network_device.h"
#include "version.h"
#include "device_settings/style.h"
#include "util.h"

extern QString button_layout;
extern QString button_home;

PreferencesWindow::PreferencesWindow()
    : QDialog(0, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    QStyle *arthurStyle = new ArthurStyle();
    setStyle(arthurStyle);

    setupUi(this);

    creditsLabel->setText(creditsLabel->text().replace("QT_VERSION", QT_VERSION_STR));
    Settings::instance().fillData(m_settingsData);

    setWindowTitle(tr("Preferences Editor"));

    updateView();
    updateCameras();

    //setWindowOpacity(.90);

    resizeEvent(0);
}

PreferencesWindow::~PreferencesWindow()
{
}

void PreferencesWindow::accept()
{
    Settings& settings = Settings::instance();
    settings.update(m_settingsData);
    settings.save();

    QStringList checkLst(settings.auxMediaRoots());
    checkLst.push_back(QDir::toNativeSeparators(settings.mediaRoot()));
    CLDeviceManager::instance().pleaseCheckDirs(checkLst);

    QDialog::accept();
}

void PreferencesWindow::updateView()
{
    versionLabel->setText(APPLICATION_VERSION);
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
        palette.setColor(QPalette::Foreground, QColor(0,0,0));
        licenseInfoLabel->setPalette(palette);

        licenseInfoLabel->setText("You have valid license installed");

        licenseButton->setEnabled(false);
    }
    else
    {
        QPalette palette = licenseInfoLabel->palette();
        palette.setColor(QPalette::Foreground, QColor(0xff,00,00));
        licenseInfoLabel->setPalette(palette);

        licenseInfoLabel->setText("You do not have valid license installed");
        licenseButton->setEnabled(true);
    }

    QList<QNetworkAddressEntry> ipv4entries = getAllIPv4AddressEntries();

    networkInterfacesList->clear();
    foreach (QNetworkAddressEntry entry, ipv4entries)
    {
        QString entryString = QString("IP Address: %1, Network Mask: %2").arg(entry.ip().toString()).arg(entry.netmask().toString());
        networkInterfacesList->addItem(entryString);
    }

    camerasList->clear();
    foreach(CameraNameAndInfo camera, m_cameras)
    {
        camerasList->addItem(camera.first);
    }

    if (ipv4entries.size() == 0)
        cameraStatusLabel->setText("No IP addresses detected. Ensure you either have static IP or there is DHCP server in your network.");
    if (ipv4entries.size() > 0 && m_cameras.size() == 0)
        cameraStatusLabel->setText("No cameras detected. If you're connected to router check that it doesn't block broadcasts.");
    else
        cameraStatusLabel->setText("");

    totalCamerasLabel->setText(QString("Total %1 cameras detected").arg(m_cameras.size()));
}

void PreferencesWindow::updateCameras()
{
    {
        CLDeviceSearcher& searcher = CLDeviceManager::instance().getDeviceSearcher();
        QMutexLocker lock(&searcher.all_devices_mtx);

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
    
    return QString("Name: %1\nCamera MAC Address: %2\nCamera IP Address: %3\nLocal IP Address: %4").arg(device->getName()).
        arg(networkDevice->getMAC()).arg(networkDevice->getIP().toString()).arg(networkDevice->getDiscoveryAddr().toString());
}

void PreferencesWindow::closeEvent(QCloseEvent * event)
{
    QMessageBox::StandardButton result = YesNoCancel(this, tr("Preferences Editor") , tr("Save changes?"));

    if (result == QMessageBox::Cancel)
    {
        event->ignore();

        return;
    }

    if (result == QMessageBox::Yes)
        accept();
}

void PreferencesWindow::resizeEvent ( QResizeEvent * event)
{
    QSize sz = this->size();
}

QString browseForDirectory()
{
    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);

    if (fileDialog.exec())
        return QDir::toNativeSeparators(fileDialog.selectedFiles().at(0));

    return "";
}

void PreferencesWindow::mainMediaFolderBrowse()
{
    QString xdir = browseForDirectory();
    if (xdir == "")
        return;

    m_settingsData.mediaRoot = xdir;

    updateView();
}

void PreferencesWindow::auxMediaFolderBrowse()
{
    QString xdir = browseForDirectory();
    if (xdir == "")
        return;

    if (m_settingsData.auxMediaRoots.indexOf(xdir) != -1)
        return;

    m_settingsData.auxMediaRoots.append(xdir);

    updateView();
}

void PreferencesWindow::auxMediaFolderRemove()
{
    QMessageBox::StandardButton result = YesNoCancel(this, tr("Preferences Editor") , tr("Really remove selected item(s) from the list?"));
    if (result == QMessageBox::Cancel)
        return;

    foreach(QListWidgetItem* item, auxMediaRootsList->selectedItems())
    {
        m_settingsData.auxMediaRoots.removeAll(QDir::fromNativeSeparators(item->text()));
    }

    updateView();
}

void PreferencesWindow::allowChangeIPChanged()
{
    m_settingsData.allowChangeIP = allowChangeIPCheckBox->isChecked();
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