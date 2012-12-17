#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QAbstractItemView>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <core/resource_managment/resource_pool.h>
#include <common/customization.h>

#include <ui/style/globals.h>


QnLicenseManagerWidget::QnLicenseManagerWidget(QWidget *parent) :
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
    connect(ui->gridLicenses,                   SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),                  this,   SLOT(at_gridLicenses_itemDoubleClicked(QTreeWidgetItem *, int)));
    connect(ui->licenseWidget,                  SIGNAL(stateChanged()),                                             this,   SLOT(at_licenseWidget_stateChanged()));

    updateLicenses();
    at_gridLicenses_currentChanged();
}

QnLicenseManagerWidget::~QnLicenseManagerWidget()
{
}

void QnLicenseManagerWidget::updateLicenses() {
    m_licenses = qnLicensePool->getLicenses();

    if (m_licenses.hardwareId().isEmpty()) {
        setEnabled(false);
    } else {
        setEnabled(true);
    }

    /* Update license widget. */
    ui->licenseWidget->setHardwareId(m_licenses.hardwareId());
    ui->licenseWidget->setFreeLicenseAvailable(!m_licenses.haveLicenseKey(qnProductFeatures().freeLicenseKey) && (qnProductFeatures().freeLicenseCount > 0));

    /* Update grid. */
    ui->gridLicenses->clear();

    int idx = 0;
    foreach(const QnLicensePtr &license, m_licenses.licenses()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::DisplayRole, license->name());
        item->setData(0, Qt::UserRole, idx++);
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
            QString text = (qnProductFeatures().freeLicenseCount > 0) ?
                tr("You do not have a valid License installed. Please activate your commercial or free license.") :
                tr("You do not have a valid License installed. Please activate your commercial license.");
            ui->infoLabel->setText(text);
            useRedLabel = true;
        }
    }

    QPalette palette = this->palette();
    if(useRedLabel)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->infoLabel->setPalette(palette);
}

void QnLicenseManagerWidget::updateFromServer(const QString &serialKey, const QString &hardwareId) {
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

void QnLicenseManagerWidget::validateLicense(const QnLicensePtr &license) {
    if (license->isValid()) {
        QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection();

        connection->addLicenseAsync(license, this, SLOT(at_licensesReceived(int,QByteArray,QnLicenseList,int)));
    } else {
        QMessageBox::warning(this, tr("License Activation"),
            tr("Invalid License. Contact our support team to get a valid License."));
    }
}

void QnLicenseManagerWidget::showLicenseDetails(const QnLicensePtr &license){
    QString details = tr("<b>Generic:</b><br />\n"
        "License Owner: %1<br />\n"
        "License key: %2<br />\n"
        "Locked to Hardware ID: %3<br />\n"
        "<br />\n"
        "<b>Features:</b><br />\n"
        "Archive Streams Allowed: %4")
        .arg(license->name())
        .arg(QLatin1String(license->key()))
        .arg(QLatin1String(license->hardwareId()))
        .arg(license->cameraCount());
    QMessageBox::information(this, tr("License Details"), details);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseManagerWidget::at_licensesReceived(int status, const QByteArray &/*errorString*/, QnLicenseList licenses, int /*handle*/)
{
    if (status != 0 || licenses.isEmpty())
    {
        QMessageBox::information(this, tr("License Activation"), tr("There was a problem activating your license."));
        return;
    }

    QnLicensePtr license = licenses.licenses().front();
    QString message = m_licenses.haveLicenseKey(license->key()) ? tr("This license is already activated.") : tr("License was successfully activated.");

    m_licenses.append(license);
    qnLicensePool->addLicense(license);

    QMessageBox::information(this, tr("License Activation"), message);
    ui->licenseWidget->setSerialKey(QString());

    updateLicenses();
}

void QnLicenseManagerWidget::at_downloadError() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        disconnect(reply, SIGNAL(finished()), this, SLOT(at_downloadFinished()));

        QMessageBox::warning(this, tr("License Activation"),
            tr("Network error has occurred during the Automatic License Activation.\nTry to activate your License manually."));

        ui->licenseWidget->setOnline(false);

        reply->deleteLater();
    }

    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_downloadFinished() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        validateLicense(QnLicensePtr(new QnLicense(QnLicense::fromString(reply->readAll()))));

        reply->deleteLater();
    }

    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_gridLicenses_currentChanged() {
    ui->detailsButton->setEnabled(ui->gridLicenses->selectionModel()->currentIndex().isValid());
}

void QnLicenseManagerWidget::at_gridLicenses_itemDoubleClicked(QTreeWidgetItem *item, int) {
    int idx = item->data(0, Qt::UserRole).toInt();
    const QnLicensePtr license = m_licenses.licenses().at(idx);
    showLicenseDetails(license);
}

void QnLicenseManagerWidget::at_licenseDetailsButton_clicked() {
    QModelIndex model = ui->gridLicenses->selectionModel()->selectedRows().front();
    if (model.column() > 0)
        model = model.sibling(model.row(), 0);
    int idx = model.data(Qt::UserRole).toInt();
    const QnLicensePtr license = m_licenses.licenses().at(idx);
    showLicenseDetails(license);
}

void QnLicenseManagerWidget::at_licenseWidget_stateChanged() {
    if(ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline()) {
        updateFromServer(ui->licenseWidget->serialKey(), QLatin1String(m_licenses.hardwareId()));
    } else {
        validateLicense(QnLicensePtr(new QnLicense(QnLicense::fromString(ui->licenseWidget->activationKey()))));
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}





