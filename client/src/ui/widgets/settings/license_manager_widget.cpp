#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"
#include "version.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QUrlQuery>

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <common/common_module.h>
#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>

#include <mustache/mustache.h>

#include <client/client_translation_manager.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>
#include <ui/models/license_list_model.h>
#include <utils/license_usage_helper.h>
#include <utils/serialization/json_functions.h>
#include <utils/common/product_features.h>
#include "managers/runtime_info_manager.h"


#define QN_LICENSE_URL "http://networkoptix.com/nolicensed_vms/activate.php"

QnLicenseManagerWidget::QnLicenseManagerWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::LicenseManagerWidget),
    m_httpClient(NULL)
{
    ui->setupUi(this);

    QList<QnLicenseListModel::Column> columns;
    columns << QnLicenseListModel::TypeColumn << QnLicenseListModel::CameraCountColumn << QnLicenseListModel::LicenseKeyColumn << QnLicenseListModel::ExpirationDateColumn << QnLicenseListModel::LicenseStatusColumn;

    m_model = new QnLicenseListModel(this);
    m_model->setColumns(columns);
    ui->gridLicenses->setModel(m_model);
    ui->gridLicenses->setSelectionBehavior(QAbstractItemView::SelectRows);

    setHelpTopic(this, Qn::SystemSettings_Licenses_Help);

    connect(ui->detailsButton,                  SIGNAL(clicked()),                                                  this,   SLOT(at_licenseDetailsButton_clicked()));
    connect(qnLicensePool,                      SIGNAL(licensesChanged()),                                          this,   SLOT(updateLicenses()));
    connect(ui->gridLicenses->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),   this,   SLOT(updateDetailsButtonEnabled()));
    connect(ui->gridLicenses,                   SIGNAL(doubleClicked(const QModelIndex &)),                         this,   SLOT(at_gridLicenses_doubleClicked(const QModelIndex &)));
    connect(ui->licenseWidget,                  SIGNAL(stateChanged()),                                             this,   SLOT(at_licenseWidget_stateChanged()));
    connect(this,                               SIGNAL(showMessageLater(QString,QString,bool)),                     this,   SLOT(showMessage(QString,QString,bool)), Qt::QueuedConnection);

    updateLicenses();
    updateDetailsButtonEnabled();
}

QnLicenseManagerWidget::~QnLicenseManagerWidget()
{
}

void QnLicenseManagerWidget::updateLicenses() {
    // do not re-read licences if we are activating one now
    if (!m_handleKeyMap.isEmpty())
        return;

    setEnabled(!ec2::QnRuntimeInfoManager::instance()->data().isEmpty());

    m_licenses = qnLicensePool->getLicenses();

    QnLicenseListHelper licenseListHelper(m_licenses);

    /* Update license widget. */
    ui->licenseWidget->setHardwareId(qnLicensePool->currentHardwareId());
    ui->licenseWidget->setFreeLicenseAvailable(!licenseListHelper.haveLicenseKey(qnProductFeatures().freeLicenseKey.toLatin1()) && (qnProductFeatures().freeLicenseCount > 0));

    /* Update grid. */
    m_model->setLicenses(m_licenses);

    /* Update info label. */
    bool useRedLabel = false;

    if (!m_licenses.isEmpty()) {
        QnLicenseUsageHelper helper;

        // TODO: #Elric #TR total mess with numerus forms, and no idea how to fix it in a sane way

        if (!helper.isValid()) {
            useRedLabel = true;
            if (helper.totalAnalog() > 0) {
                ui->infoLabel->setText(
                    tr("The software is licensed to %1 cameras and %2 analog cameras.\nAt least %3 licenses are required.")
                        .arg(helper.totalDigital())
                        .arg(helper.totalAnalog())
                        .arg(helper.required())
                );
            } else {
                ui->infoLabel->setText(
                    tr("The software is licensed to %1 cameras.\nAt least %3 licenses are required.")
                        .arg(helper.totalDigital())
                        .arg(helper.required())
                );
            }
        } else {
            if (helper.totalAnalog() > 0) {
                ui->infoLabel->setText(
                    tr("The software is licensed to %1 cameras and %2 analog cameras.\n%3 licenses and %4 analog licenses are currently in use.")
                        .arg(helper.totalDigital())
                        .arg(helper.totalAnalog())
                        .arg(helper.usedDigital())
                        .arg(helper.usedAnalog())
                );
            } else {
                ui->infoLabel->setText(
                    tr("The software is licensed to %1 cameras.\n%2 licenses are currently in use.")
                        .arg(helper.totalDigital())
                        .arg(helper.usedDigital())
                );
            }
        }
    } else {
        if (qnLicensePool->currentHardwareId().isEmpty()) {
            ui->infoLabel->setText(tr("Obtaining licenses from Enterprise Controller..."));
            useRedLabel = false;
        } else {
            QString text = (qnProductFeatures().freeLicenseCount > 0) ?
                tr("You do not have a valid license installed. Please activate your commercial or trial license.") :
                tr("You do not have a valid license installed. Please activate your commercial license.");
            ui->infoLabel->setText(text);
            useRedLabel = true;
        }
    }

    QPalette palette = this->palette();
    if(useRedLabel)
        setWarningStyle(&palette);
    ui->infoLabel->setPalette(palette);
}

void QnLicenseManagerWidget::showMessage(const QString &title, const QString &message, bool warning) {
    if (warning)
        QMessageBox::warning(this, title, message);
    else
        QMessageBox::information(this, title, message);
}

void QnLicenseManagerWidget::updateFromServer(const QByteArray &licenseKey, const QList<QByteArray> &mainHardwareIds, const QList<QByteArray> &compatibleHardwareIds) {
    if (!m_httpClient)
        m_httpClient = new QNetworkAccessManager(this);

    QUrl url(QLatin1String(QN_LICENSE_URL));
    QNetworkRequest request;
    request.setUrl(url.toString());

    QUrlQuery params;
    params.addQueryItem(QLatin1String("license_key"), QLatin1String(licenseKey));

    int n = 1;
    foreach (const QByteArray& licenseKey, QnLicenseListHelper(m_licenses).allLicenseKeys() ) {
        params.addQueryItem(QString(QLatin1String("license_key%1")).arg(n), QLatin1String(licenseKey));
        n++;
    }

    int hw = 0;
    foreach (const QByteArray& hwid, mainHardwareIds) {
        QString name;
        if (hw == 0)
            name = QLatin1String("oldhwid");
        else if (hw == 1)
            name = QLatin1String("hwid");
        else
            name = QString(QLatin1String("hwid%1")).arg(hw);

        params.addQueryItem(name, QLatin1String(hwid));

        hw++;
    }

    hw = 1;
    foreach (const QByteArray& hwid, compatibleHardwareIds) {
        QString name = QString(QLatin1String("chwid%1")).arg(hw);
        params.addQueryItem(name, QLatin1String(hwid));
        hw++;
    }

    params.addQueryItem(QLatin1String("brand"), QLatin1String(QN_PRODUCT_NAME_SHORT));
    params.addQueryItem(QLatin1String("version"), QLatin1String(QN_ENGINE_VERSION));
    params.addQueryItem(QLatin1String("lang"), qnCommon->instance<QnClientTranslationManager>()->getCurrentLanguage());

    QNetworkReply *reply = m_httpClient->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    m_replyKeyMap[reply] = licenseKey;

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_downloadError()));
    connect(reply, SIGNAL(finished()), this, SLOT(at_downloadFinished()));
}

void QnLicenseManagerWidget::validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses) {
    QList<QnLicensePtr> licensesToUpdate;
    QnLicensePtr keyLicense;

    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    foreach (const QnLicensePtr& license, licenses) {
        if (!license)
            continue;

        if (license->key() == licenseKey)
            keyLicense = license;

        QnLicensePtr ownLicense = licenseListHelper.getLicenseByKey(license->key());
        if (!ownLicense || ownLicense->signature() != license->signature())
            licensesToUpdate.append(license);
    }

    if (!licensesToUpdate.isEmpty()) {
        auto addLisencesHandler = [this, licensesToUpdate]( int reqID, ec2::ErrorCode errorCode ){
            at_licensesReceived( reqID, errorCode, licensesToUpdate );
        };
        int handle = QnAppServerConnectionFactory::getConnection2()->getLicenseManager()->addLicenses(
            licensesToUpdate, this, addLisencesHandler );
        m_handleKeyMap[handle] = licenseKey;
    }

    if (!keyLicense) {
        /* QNetworkReply slots should not start event loop. */
        emit showMessageLater(tr("License Activation"),
                              tr("Invalid License. Please contact our support team to get a valid license."),
                              true);
    } else if (licenseListHelper.getLicenseByKey(licenseKey)) {
        emit showMessageLater(tr("License Activation"),
                              tr("The license is already activated."),
                              true);
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
        .arg(license->typeName())
        .arg(QLatin1String(license->key()))
        .arg(QLatin1String(qnLicensePool->currentHardwareId()))
        .arg(license->cameraCount());
    QMessageBox::information(this, tr("License Details"), details);
}

void QnLicenseManagerWidget::updateDetailsButtonEnabled() {
    ui->detailsButton->setEnabled(ui->gridLicenses->selectionModel()->currentIndex().isValid());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseManagerWidget::at_licensesReceived(int handle, ec2::ErrorCode errorCode, QnLicenseList licenses)
{
    if (!m_handleKeyMap.contains(handle))
        return;

    QByteArray licenseKey = m_handleKeyMap[handle];
    m_handleKeyMap.remove(handle);

    QnLicenseListHelper licenseListHelper(licenses);
    QnLicensePtr license = licenseListHelper.getLicenseByKey(licenseKey);
        
    QString message;
    if (!license || (errorCode != ec2::ErrorCode::ok))
        message = tr("There was a problem activating your license.");
    else if (license)
        message = tr("License was successfully activated.");

    if (!licenses.isEmpty()) {
        m_licenses.append(licenses);
        qnLicensePool->addLicenses(licenses);
    }

    if (!message.isEmpty())
        QMessageBox::information(this, tr("License Activation"), message);

    ui->licenseWidget->setSerialKey(QString());

    updateLicenses();
}

void QnLicenseManagerWidget::at_downloadError() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        disconnect(reply, 0, this, 0); //avoid double "onError" handling
        m_replyKeyMap.remove(reply);
        reply->deleteLater();

        /* QNetworkReply slots should not start eventLoop */
        emit showMessageLater(tr("License Activation ") + reply->errorString(),
                              tr("Network error has occurred during automatic license activation.\nTry to activate your license manually."),
                              true);

        ui->licenseWidget->setOnline(false);
    }
    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_downloadFinished() {
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        QList<QnLicensePtr> licenses;

        QByteArray replyData = reply->readAll();

        // TODO: #Elric use JSON mapping here.
        // If we can deserialize JSON it means there is an error.
        QJsonObject errorMessage;
        if (QJson::deserialize(replyData, &errorMessage)) {
//            QString error = errorMessage.value(lit("error")).toString();
            QString messageId = errorMessage.value(lit("messageId")).toString();
            QString message = errorMessage.value(lit("message")).toString();
            QVariantMap arguments = errorMessage.value(lit("arguments")).toObject().toVariantMap();

            if(messageId == lit("DatabaseError")) {
                message = tr("Database error has occurred.");
            } else if(messageId == lit("InvalidData")) {
                message = tr("Invalid data was received.");
            } else if(messageId == lit("InvalidKey")) {
                message = tr("The license key is invalid.");
            } else if(messageId == lit("InvalidBrand")) {
                message = tr("You are trying to activate an incompatible license with your software.");
            } else if(messageId == lit("AlreadyActivated")) {
                message = tr("This license key has been previously activated to hardware id {{hwid}} on {{time}}.");
            }

            message = Mustache::renderTemplate(message, arguments);

            /* QNetworkReply slots should not start eventLoop */
            emit showMessageLater(tr("License Activation"),
                                  tr("There was a problem activating your license.") + lit(" ") + message,
                                  true);
            ui->licenseWidget->setState(QnLicenseWidget::Normal);

            return;
        }

        QTextStream is(&replyData);
        is.setCodec("UTF-8");

        while (!is.atEnd()) {
            QnLicensePtr license = QnLicense::readFromStream(is);
            if (!license )
                break;

            QnLicense::ErrorCode errCode = QnLicense::NoError;
            if (license->isValid(QLatin1String(QN_PRODUCT_NAME_SHORT), &errCode))
                licenses.append(license);
            else if (errCode == QnLicense::Expired)
                licenses.append(license); // ignore expired error code
        }

        validateLicenses(m_replyKeyMap[reply], licenses);

        m_replyKeyMap.remove(reply);

        reply->deleteLater();
    }

    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_gridLicenses_doubleClicked(const QModelIndex &index) {
    showLicenseDetails(m_model->license(index));
}

void QnLicenseManagerWidget::at_licenseDetailsButton_clicked() {
    QModelIndex index = ui->gridLicenses->selectionModel()->currentIndex();
    showLicenseDetails(m_model->license(index));
}

void QnLicenseManagerWidget::at_licenseWidget_stateChanged() {
    if(ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline()) {
        updateFromServer(ui->licenseWidget->serialKey().toLatin1(), qnLicensePool->mainHardwareIds(), qnLicensePool->compatibleHardwareIds());
    } else {
        QList<QnLicensePtr> licenseList;
        QnLicensePtr license(new QnLicense(ui->licenseWidget->activationKey()));
        if (license->isValid(QLatin1String(QN_PRODUCT_NAME_SHORT)))
            licenseList.append(license);

        validateLicenses(license ? license->key() : "", licenseList);
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}





