#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <core/resourcemanagment/resource_pool.h>

#include <ui/style/globals.h>


LicenseManagerWidget::LicenseManagerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LicenseManagerWidget),
    m_httpClient(NULL)
{
    ui->setupUi(this);

    ui->detailsButton->setEnabled(false);
    ui->gridLicenses->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(ui->detailsButton,                  SIGNAL(clicked()),                                                  this,   SLOT(at_licenseDetailsButton_clicked()));
    connect(qnLicensePool,                      SIGNAL(licensesChanged()),                                          this,   SLOT(updateLicenses()));
    connect(ui->gridLicenses->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),   this,   SLOT(at_gridLicenses_currentChanged()));
    connect(ui->licenseWidget,                  SIGNAL(stateChanged()),                                             this,   SLOT(at_licenseWidget_stateChanged()));

    updateLicenses();
    at_gridLicenses_currentChanged();
}

LicenseManagerWidget::~LicenseManagerWidget()
{
}

void LicenseManagerWidget::updateLicenses() {
    m_licenses = qnLicensePool->getLicenses();

    if (m_licenses.hardwareId().isEmpty()) {
        setEnabled(false);
    } else {
        setEnabled(true);
    }

    /* Update license widget. */
    ui->licenseWidget->setHardwareId(m_licenses.hardwareId());
    ui->licenseWidget->setFreeLicenseAvailable(!m_licenses.haveLicenseKey(QnLicense::FREE_LICENSE_KEY));

    /* Update grid. */
    ui->gridLicenses->clear();

    foreach(const QnLicensePtr &license, m_licenses.licenses()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::DisplayRole, license->name());
        item->setData(1, Qt::DisplayRole, license->cameraCount());
        item->setData(2, Qt::DisplayRole, license->key());
        ui->gridLicenses->addTopLevelItem(item);
    }

    /* Update info label. */
    bool useRedLabel = false;

    if (!m_licenses.isEmpty()) {
        int totalCameras = m_licenses.totalCameras();
        int usingCameras = qnResPool->activeCameras();
        ui->infoLabel->setText(QString(tr("The software is licensed to %1 cameras. Currently using %2.")).arg(totalCameras).arg(usingCameras));
        useRedLabel = usingCameras > totalCameras;
    } else {
        if (m_licenses.hardwareId().isEmpty()) {
            ui->infoLabel->setText(tr("Obtaining licenses from Enterprise Controller..."));
            useRedLabel = false;
        } else {
            ui->infoLabel->setText(tr("You do not have a valid License installed. Please activate your commercial or free license."));
            useRedLabel = true;
        }
    }

    QPalette palette = this->palette();
    if(useRedLabel)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->infoLabel->setPalette(palette);
}

void LicenseManagerWidget::updateFromServer(const QString &serialKey, const QString &hardwareId) {
    if (!m_httpClient)
        m_httpClient = new QNetworkAccessManager(this);

    QUrl url(QLatin1String("http://networkoptix.com/nolicensed_vms/activate.php"));

    QNetworkRequest request;
    request.setUrl(url);

    QUrl params;
    params.addQueryItem(QLatin1String("license_key"), serialKey);
    params.addQueryItem(QLatin1String("hwid"), hardwareId);

    QNetworkReply *reply = m_httpClient->post(request, params.encodedQuery());

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_downloadError()));
    connect(reply, SIGNAL(finished()), this, SLOT(at_downloadFinished()));
}

void LicenseManagerWidget::validateLicense(const QnLicensePtr &license) {
    if (license->isValid()) {
        QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection();

        connection->addLicenseAsync(license, this, SLOT(at_licensesReceived(int,QByteArray,QnLicenseList,int)));
    } else {
        QMessageBox::warning(this, tr("License Activation"),
            tr("Invalid License. Contact our support team to get a valid License."));
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void LicenseManagerWidget::at_licensesReceived(int status, const QByteArray &/*errorString*/, QnLicenseList licenses, int /*handle*/)
{
    if (status != 0 || licenses.isEmpty())
    {
        QMessageBox::information(this, tr("License Activation"), tr("There was a problem activating your license."));
        return;
    }

    QnLicensePtr license = licenses.licenses().front();
    QString message = m_licenses.haveLicenseKey(license->key()) ? tr("This license is already activated.") : tr("License was succesfully activated.");

    m_licenses.append(license);
    qnLicensePool->addLicense(license);

    QMessageBox::information(this, tr("License Activation"), message);
    ui->licenseWidget->setSerialKey(QString());
}

void LicenseManagerWidget::at_downloadError() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        disconnect(reply, SIGNAL(finished()), this, SLOT(at_downloadFinished()));

        QMessageBox::warning(this, tr("License Activation"),
            tr("Network error has occurred during the Automatic License Activation.\nTry to activate your License manually."));

        ui->licenseWidget->setOnline(false);

        reply->deleteLater();
    }

    ui->licenseWidget->setState(LicenseWidget::Normal);
}

void LicenseManagerWidget::at_downloadFinished() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        validateLicense(QnLicensePtr(new QnLicense(QnLicense::fromString(reply->readAll()))));

        reply->deleteLater();
    }

    ui->licenseWidget->setState(LicenseWidget::Normal);
}

void LicenseManagerWidget::at_gridLicenses_currentChanged() {
    ui->detailsButton->setEnabled(ui->gridLicenses->selectionModel()->currentIndex().isValid());
}

void LicenseManagerWidget::at_licenseDetailsButton_clicked() {
    const QnLicensePtr license = m_licenses.licenses().at(ui->gridLicenses->selectionModel()->selectedRows().front().row());

    QString details = tr("<b>Generic:</b><br />\n"
        "License Owner: %1<br />\n"
        "Serial Key: %2<br />\n"
        "Locked to Hardware ID: %3<br />\n"
        "<br />\n"
        "<b>Features:</b><br />\n"
        "Archive Streams Allowed: %4")
        .arg(license->name().constData())
        .arg(license->key().constData())
        .arg(license->hardwareId().constData())
        .arg(license->cameraCount());
    QMessageBox::information(this, tr("License Details"), details);
}

void LicenseManagerWidget::at_licenseWidget_stateChanged() {
    if(ui->licenseWidget->state() != LicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline()) {
        updateFromServer(ui->licenseWidget->serialKey(), m_licenses.hardwareId());
    } else {
        validateLicense(QnLicensePtr(new QnLicense(QnLicense::fromString(ui->licenseWidget->activationKey()))));
        ui->licenseWidget->setState(LicenseWidget::Normal);
    }
}





