#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>

#include <QtGui/QAbstractItemView>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <core/resource_managment/resource_pool.h>
#include <common/customization.h>

#include <ui/style/warning_style.h>
#include <utils/license_usage_helper.h>

namespace {

    QString getLicenseName(QnLicensePtr license) {

        if (license->key() == qnProductFeatures().freeLicenseKey.toAscii())
            return QObject::tr("Free");

        if (!license->expiration().isEmpty())
            return QObject::tr("Trial");

        if (license->xclass().toLower() == QLatin1String("analog"))
            return QObject::tr("Analog");
        return QObject::tr("Enterprise");
    }

    enum Columns {
        NameColumn,
        //ClassColumn,
        CameraCountColumn,
        KeyColumn,
        ExpirationDateColumn,
        ColumnCount
    };

}


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
    ui->licenseWidget->setFreeLicenseAvailable(!m_licenses.haveLicenseKey(qnProductFeatures().freeLicenseKey.toAscii()) && (qnProductFeatures().freeLicenseCount > 0));

    /* Update grid. */
    ui->gridLicenses->clear();

    int idx = 0;
    foreach(const QnLicensePtr &license, m_licenses.licenses()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(NameColumn, getLicenseName(license));
        item->setData(NameColumn, Qt::UserRole, idx++);
        //item->setText(ClassColumn, license->xclass().toLower());
        item->setText(CameraCountColumn, QString::number(license->cameraCount()));
        item->setText(KeyColumn, QString::fromLatin1(license->key()));
        item->setText(ExpirationDateColumn, license->expiration().isEmpty() ? tr("Never") : license->expiration());
        ui->gridLicenses->addTopLevelItem(item);
    }

    /* Update info label. */
    bool useRedLabel = false;

    if (!m_licenses.isEmpty()) {
        QnLicenseUsageHelper helper;

        if (!helper.isValid()) {
            useRedLabel = true;
            ui->infoLabel->setText(QString(tr("The software is licensed to %1 digital and %2 analog cameras.\n"\
                                              "Required at least %3 digital and %4 analog."))
                                   .arg(helper.totalDigital())
                                   .arg(helper.totalAnalog())
                                   .arg(helper.requiredDigital())
                                   .arg(helper.requiredAnalog()));

        } else {
            ui->infoLabel->setText(QString(tr("The software is licensed to %1 digital and %2 analog cameras.\n"\
                                              "Currently using %3 digital and %4 analog."))
                                   .arg(helper.totalDigital())
                                   .arg(helper.totalAnalog())
                                   .arg(helper.usedDigital())
                                   .arg(helper.usedAnalog()));

        }
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
        setWarningStyle(&palette);
    ui->infoLabel->setPalette(palette);
}

void QnLicenseManagerWidget::updateFromServer(const QByteArray &licenseKey, const QString &hardwareId, const QString &oldHardwareId) {
    if (!m_httpClient)
        m_httpClient = new QNetworkAccessManager(this);

//    QUrl url(QLatin1String("http://networkoptix.com/nolicensed_vms/activate.php"));
    QUrl url(QLatin1String("http://noptix.enk.me/~ivan_vigasin/new/vms.dev/activate.php"));

    QNetworkRequest request;
    request.setUrl(url);

    QUrl params;
    params.addQueryItem(QLatin1String("license_key"), QLatin1String(licenseKey));

    int n = 1;
    foreach (const QByteArray& licenseKey, m_licenses.allLicenseKeys() ) {
        params.addQueryItem(QString(QLatin1String("license_key%1")).arg(n), QLatin1String(licenseKey));
        n++;
    }

    params.addQueryItem(QLatin1String("hwid"), hardwareId);
    params.addQueryItem(QLatin1String("oldhwid"), oldHardwareId);

    QNetworkReply *reply = m_httpClient->post(request, params.encodedQuery());  
    m_replyKeyMap[reply] = licenseKey;

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_downloadError()));
    connect(reply, SIGNAL(finished()), this, SLOT(at_downloadFinished()));
}

void QnLicenseManagerWidget::validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses) {
    QList<QnLicensePtr> licensesToUpdate;
    QnLicensePtr keyLicense;

    foreach (const QnLicensePtr& license, licenses) {
        if (!license)
            continue;

        if (license->key() == licenseKey)
            keyLicense = license;

        QnLicensePtr ownLicense = qnLicensePool->getLicenses().getLicenseByKey(license->key());
        if (!ownLicense || ownLicense->signature() != license->signature())
            licensesToUpdate.append(license);
    }

    if (!licensesToUpdate.isEmpty()) {
        QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection();
        int handle = connection->addLicensesAsync(licensesToUpdate, this, SLOT(at_licensesReceived(int,QByteArray,QnLicenseList,int)));
        m_handleKeyMap[handle] = licenseKey;
    }

    if (!keyLicense) {
        QMessageBox::warning(this, tr("License Activation"),
            tr("Invalid License. Contact our support team to get a valid License."));
    }
}

void QnLicenseManagerWidget::showLicenseDetails(const QnLicensePtr &license){
    QString details = tr("<b>Generic:</b><br />\n"
        "License Type: %1<br />\n"
        "License Key: %2<br />\n"
        "Locked to Hardware ID: %3<br />\n"
        "<br />\n"
        "<b>Features:</b><br />\n"
        "Archive Streams Allowed: %4")
        .arg(getLicenseName(license))
        .arg(QLatin1String(license->key()))
        .arg(QLatin1String(m_licenses.hardwareId()))
        .arg(license->cameraCount());
    QMessageBox::information(this, tr("License Details"), details);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseManagerWidget::at_licensesReceived(int status, const QByteArray &/*errorString*/, QnLicenseList licenses, int handle)
{
    QByteArray licenseKey = m_handleKeyMap[handle];
    m_handleKeyMap.remove(handle);

    QnLicensePtr license = licenses.getLicenseByKey(licenseKey);
        
    QString message;
    if (!license || status)
        message = tr("There was a problem activating your license.");
    else
        message = m_licenses.haveLicenseKey(license->key()) ? tr("This license is already activated.") : tr("License was successfully activated.");

    if (!licenses.isEmpty()) {
        m_licenses.append(licenses);
        qnLicensePool->addLicenses(licenses);
    }

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

        m_replyKeyMap.remove(reply);
        reply->deleteLater();
    }

    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_downloadFinished() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        QList<QnLicensePtr> licenses;

        QByteArray replyData = reply->readAll();
        QTextStream is(&replyData);
        is.setCodec("UTF-8");

        while (!is.atEnd()) {
            QnLicensePtr license = readLicenseFromStream(is);
            if (!license )
                break;

            if (license->isValid(qnLicensePool->getLicenses().hardwareId()))
                licenses.append(license);
        }

        validateLicenses(m_replyKeyMap[reply], licenses);

        m_replyKeyMap.remove(reply);

        reply->deleteLater();
    }

    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_gridLicenses_currentChanged() {
    ui->detailsButton->setEnabled(ui->gridLicenses->selectionModel()->currentIndex().isValid());
}

void QnLicenseManagerWidget::at_gridLicenses_itemDoubleClicked(QTreeWidgetItem *item, int) {
    int idx = item->data(NameColumn, Qt::UserRole).toInt();
    const QnLicensePtr license = m_licenses.licenses().at(idx);
    showLicenseDetails(license);
}

void QnLicenseManagerWidget::at_licenseDetailsButton_clicked() {
    QModelIndex index = ui->gridLicenses->selectionModel()->selectedRows().front();
    if (index.column() > NameColumn)
        index = index.sibling(index.row(), NameColumn);
    int idx = index.data(Qt::UserRole).toInt();
    const QnLicensePtr license = m_licenses.licenses().at(idx);
    showLicenseDetails(license);
}

void QnLicenseManagerWidget::at_licenseWidget_stateChanged() {
    if(ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline()) {
        updateFromServer(ui->licenseWidget->serialKey().toLatin1(), QLatin1String(m_licenses.hardwareId()), QLatin1String(m_licenses.oldHardwareId()));
    } else {
        QList<QnLicensePtr> licenseList;
        QnLicensePtr license(new QnLicense(ui->licenseWidget->activationKey()));
        licenseList.append(license);
        validateLicenses(license ? license->key() : "", licenseList);
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}





