#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"
#include <utils/common/app_info.h>

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
#include <ui/dialogs/license_details_dialog.h>
#include <ui/dialogs/message_box.h>

#include <utils/license_usage_helper.h>
#include <utils/serialization/json_functions.h>
#include <utils/common/product_features.h>
#include "api/runtime_info_manager.h"

QnLicenseManagerWidget::QnLicenseManagerWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::LicenseManagerWidget),
    m_camerasUsageWatcher(new QnCamLicenseUsageWatcher()),
    m_videowallUsageWatcher(new QnVideoWallLicenseUsageWatcher()),
    m_httpClient(NULL)
{
    ui->setupUi(this);

    QList<QnLicenseListModel::Column> columns;
    columns 
        << QnLicenseListModel::TypeColumn 
        << QnLicenseListModel::CameraCountColumn 
        << QnLicenseListModel::LicenseKeyColumn 
        << QnLicenseListModel::ExpirationDateColumn
        << QnLicenseListModel::ServerColumn
        << QnLicenseListModel::LicenseStatusColumn
        ;

    m_model = new QnLicenseListModel(this);
    m_model->setColumns(columns);

    ui->gridLicenses->setModel(m_model);
    ui->gridLicenses->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->gridLicenses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    setHelpTopic(this, Qn::SystemSettings_Licenses_Help);

    connect(ui->detailsButton,                  SIGNAL(clicked()),                                                  this,   SLOT(at_licenseDetailsButton_clicked()));
    connect(ui->removeButton,                   SIGNAL(clicked()),                                                  this,   SLOT(at_removeButton_clicked()));
    connect(qnLicensePool,                      SIGNAL(licensesChanged()),                                          this,   SLOT(updateLicenses()));
    connect(ui->gridLicenses->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),   this,   SLOT(updateDetailsButtonEnabled()));
    connect(ui->gridLicenses,                   SIGNAL(doubleClicked(const QModelIndex &)),                         this,   SLOT(at_gridLicenses_doubleClicked(const QModelIndex &)));
    connect(ui->licenseWidget,                  SIGNAL(stateChanged()),                                             this,   SLOT(at_licenseWidget_stateChanged()));
    connect(this,                               SIGNAL(showMessageLater(QString,QString,bool)),                     this,   SLOT(showMessage(QString,QString,bool)), Qt::QueuedConnection);

    auto updateLicensesIfNeeded = [this] {
        if (!isVisible())
            return;
        updateLicenses();
    };

    connect(m_camerasUsageWatcher,              &QnLicenseUsageWatcher::licenseUsageChanged,                        this,   updateLicensesIfNeeded);
    connect(m_videowallUsageWatcher,            &QnLicenseUsageWatcher::licenseUsageChanged,                        this,   updateLicensesIfNeeded);

    updateLicenses();
}

QnLicenseManagerWidget::~QnLicenseManagerWidget()
{
}

void QnLicenseManagerWidget::showEvent(QShowEvent *event) {
    base_type::showEvent(event);
}


void QnLicenseManagerWidget::updateLicenses() {
    // do not re-read licenses if we are activating one now
    if (!m_handleKeyMap.isEmpty())
        return;

    bool connected = false;
    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems()) {
        if (info.data.peer.peerType != Qn::PT_Server)
            continue;
        connected = true;
        break;
    }

    setEnabled(connected);

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
        qDebug() << "LICENSES UPDATED";

        // TODO: #Elric #TR total mess with numerous forms, and no idea how to fix it in a sane way

        QString msg(tr("The software is licensed to: "));

        QnCamLicenseUsageHelper camUsageHelper;
        QnVideoWallLicenseUsageHelper vwUsageHelper;
        QList<QnLicenseUsageHelper*> helpers;
        helpers 
            << &camUsageHelper
            << &vwUsageHelper
            ;

        foreach (QnLicenseUsageHelper* helper, helpers) {
            foreach (Qn::LicenseType lt, helper->licenseTypes()) {
                if (helper->totalLicenses(lt) > 0)
                    msg += tr("\n%1 %2").arg(helper->totalLicenses(lt)).arg(QnLicense::longDisplayName(lt));
            }
        }

        foreach (QnLicenseUsageHelper *helper, helpers) {
            if (!helper->isValid()) 
            {
                useRedLabel = true;
                foreach (Qn::LicenseType lt, helper->licenseTypes()) {
                    if (helper->usedLicenses(lt) > 0)
                        msg += tr("\nAt least %n %2 are required", "", helper->usedLicenses(lt)).arg(QnLicense::longDisplayName(lt));
                }
            }
            else {
                foreach (Qn::LicenseType lt, helper->licenseTypes()) {
                    if (helper->usedLicenses(lt) > 0)
                        msg += tr("\n%n %2 are currently in use", "", helper->usedLicenses(lt)).arg(QnLicense::longDisplayName(lt));
                }
            }
        }
        ui->infoLabel->setText(msg);
    } else {
        if (qnLicensePool->currentHardwareId().isEmpty()) {
            ui->infoLabel->setText(tr("Obtaining licenses from Server..."));
            useRedLabel = false;
        } else {
            QString text = (qnProductFeatures().freeLicenseCount > 0) ?
                tr("You do not have a valid license installed.\nPlease activate your commercial or trial license.") :
                tr("You do not have a valid license installed.\nPlease activate your commercial license.");
            ui->infoLabel->setText(text);
            useRedLabel = true;
        }
    }

    QPalette palette = this->palette();
    if(useRedLabel)
        setWarningStyle(&palette);
    ui->infoLabel->setPalette(palette);

    updateDetailsButtonEnabled();
}

void QnLicenseManagerWidget::showMessage(const QString &title, const QString &message, bool warning) {
    QScopedPointer<QnWorkbenchStateDependentDialog<QnMessageBox>> messageBox(new QnWorkbenchStateDependentDialog<QnMessageBox>(this));
    messageBox->setIcon(warning ? QMessageBox::Warning : QMessageBox::Information);
    messageBox->setWindowTitle(title);
    messageBox->setText(message);
    QPushButton* copyButton = messageBox->addCustomButton(tr("Copy to Clipboard"), QMessageBox::HelpRole);
    connect(copyButton, &QPushButton::clicked, this, [this, message]{
        qApp->clipboard()->setText(message);
    });
    messageBox->setStandardButtons(QMessageBox::Ok);
    messageBox->setEscapeButton(QMessageBox::Ok);
    messageBox->setDefaultButton(QMessageBox::Ok);
    messageBox->exec();
}

void QnLicenseManagerWidget::updateFromServer(const QByteArray &licenseKey, bool infoMode, const QUrl &url) 
{
    const QList<QByteArray> mainHardwareIds = qnLicensePool->mainHardwareIds();
    const QList<QByteArray> compatibleHardwareIds = qnLicensePool->compatibleHardwareIds();

    if (!QnRuntimeInfoManager::instance()->items()->hasItem(qnCommon->remoteGUID())) {
        emit showMessageLater(tr("License Activation"),
            tr("Network error has occurred during automatic license activation.\nTry to activate your license manually."),
            true);
        ui->licenseWidget->setOnline(false);
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }

    if (!m_httpClient)
        m_httpClient = new QNetworkAccessManager(this);

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
    if (infoMode)
        params.addQueryItem(lit("mode"), lit("info"));

    hw = 1;
    foreach (const QByteArray& hwid, compatibleHardwareIds) {
        QString name = QString(QLatin1String("chwid%1")).arg(hw);
        params.addQueryItem(name, QLatin1String(hwid));
        hw++;
    }

    ec2::ApiRuntimeData runtimeData = QnRuntimeInfoManager::instance()->items()->getItem(qnCommon->remoteGUID()).data;

    params.addQueryItem(QLatin1String("box"), runtimeData.box);
    params.addQueryItem(QLatin1String("brand"), runtimeData.brand);
    params.addQueryItem(QLatin1String("version"), qnCommon->engineVersion().toString());
    params.addQueryItem(QLatin1String("lang"), qnCommon->instance<QnClientTranslationManager>()->getCurrentLanguage());

    QNetworkReply *reply = m_httpClient->post(request, params.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_downloadError()));
    connect(reply, &QNetworkReply::finished, this, [this, licenseKey, infoMode, url, reply] {
        QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).value<QUrl>();
        if (!redirectUrl.isEmpty()) {
            updateFromServer(licenseKey, infoMode, redirectUrl);    //TODO: #GDM add possible redirect loop check
            return;
        }
        processReply(reply, licenseKey, url, infoMode);
    });
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

    /* There is an issue when we are trying to activate and Edge license on the PC. 
     * In this case activation server will return no error but local check will fail.  */
    if (!keyLicense) {
        /* QNetworkReply slots should not start event loop. */
        emit showMessageLater(tr("License Activation"),
            tr("You are trying to activate an incompatible license with your software. "
                "Please contact support team to get a valid license key."),
                              true);
    } else if (licenseListHelper.getLicenseByKey(licenseKey)) {
        emit showMessageLater(tr("License Activation"),
                              tr("The license is already activated."),
                              true);
    }
}

void QnLicenseManagerWidget::showLicenseDetails(const QnLicensePtr &license) {
    if (!license)
        return;

    QScopedPointer<QnLicenseDetailsDialog> dialog(new QnLicenseDetailsDialog(license, this));
    dialog->exec();
}

void QnLicenseManagerWidget::updateDetailsButtonEnabled() 
{
    QModelIndex idx = ui->gridLicenses->selectionModel()->currentIndex();
    ui->detailsButton->setEnabled(idx.isValid());
    QnLicensePtr license = m_model->license(idx);
    ui->removeButton->setEnabled(license && !license->isValid());
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
        message = tr("There was a problem activating your license key. Network error has occurred.");
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
        disconnect(reply, NULL, this, NULL); //avoid double "onError" handling
        reply->deleteLater();

        /* QNetworkReply slots should not start eventLoop */
        emit showMessageLater(tr("License Activation ") + reply->errorString(),
            tr("Network error has occurred during automatic license activation. "
               "Please contact support team to activate your license key manually."),
            true);

        ui->licenseWidget->setOnline(false);
    }
    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::processReply(QNetworkReply *reply, const QByteArray& licenseKey, const QUrl &url, bool infoMode) {
    QList<QnLicensePtr> licenses;

    QByteArray replyData = reply->readAll();

    // TODO: #Elric use JSON mapping here.
    // If we can deserialize JSON it means there is an error.
    QJsonObject errorMessage;
    if (QJson::deserialize(replyData, &errorMessage)) 
    {
        QString message = QnLicenseUsageHelper::activationMessage(errorMessage);
        /* QNetworkReply slots should not start eventLoop */
        emit showMessageLater(tr("License Activation"),
            message,
            true);
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
        return;
    }

    QTextStream is(&replyData);
    is.setCodec("UTF-8");
    reply->deleteLater();

    while (!is.atEnd()) {
        QnLicensePtr license = QnLicense::readFromStream(is);
        if (!license )
            break;

        QnLicense::ErrorCode errCode = QnLicense::NoError;

        if (infoMode) {
            if (!license->isValid(&errCode, QnLicense::VM_CheckInfo) && errCode != QnLicense::Expired) {
                emit showMessageLater(tr("License activation"), tr("Can't activate license:  %1").arg(QnLicense::errorMessage(errCode)), true);
                ui->licenseWidget->setState(QnLicenseWidget::Normal);
            }
            else {
                updateFromServer(license->key(), false, url);
            }

            return;
        }
        else {
            if (license->isValid(&errCode, QnLicense::VM_JustCreated))
                licenses.append(license);
            else if (errCode == QnLicense::Expired)
                licenses.append(license); // ignore expired error code
        }
    }

    validateLicenses(licenseKey, licenses);


    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_gridLicenses_doubleClicked(const QModelIndex &index) {
    showLicenseDetails(m_model->license(index));
}

void QnLicenseManagerWidget::at_licenseDetailsButton_clicked() {
    QModelIndex index = ui->gridLicenses->selectionModel()->currentIndex();
    showLicenseDetails(m_model->license(index));
}

void QnLicenseManagerWidget::at_removeButton_clicked() 
{
    QModelIndex index = ui->gridLicenses->selectionModel()->currentIndex();
    QnLicensePtr license = m_model->license(index);
    if (!license)
        return;

    auto removeLisencesHandler = [this, license]( int reqID, ec2::ErrorCode errorCode ) {
        at_licenseRemoved( reqID, errorCode, license );
    };

    int handle = QnAppServerConnectionFactory::getConnection2()->getLicenseManager()->removeLicense(license, this,  removeLisencesHandler);
}

void QnLicenseManagerWidget::at_licenseRemoved(int reqID, ec2::ErrorCode errorCode, QnLicensePtr license)
{
    if (errorCode == ec2::ErrorCode::ok) {
        QModelIndex index = ui->gridLicenses->selectionModel()->currentIndex();
        ui->gridLicenses->model()->removeRow(index.row());
    }
    else {
        emit showMessageLater(tr("Remove license"), tr("Can't remove license from server:  %1").arg(ec2::toString(errorCode)), true);
    }
    updateLicenses();
}

void QnLicenseManagerWidget::at_licenseWidget_stateChanged() {
    if(ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline()) {
        updateFromServer(ui->licenseWidget->serialKey().toLatin1(), true, QUrl(QN_LICENSE_URL));
    } else {
        QnLicensePtr license(new QnLicense(ui->licenseWidget->activationKey()));

        QnLicense::ErrorCode errCode = QnLicense::NoError;
        if (license->isValid(&errCode, QnLicense::VM_JustCreated) || errCode == QnLicense::Expired) {
            validateLicenses(license->key(), QList<QnLicensePtr>() << license);
        }
        else {
            QString message;
            switch (errCode) {
            case QnLicense::InvalidSignature:
                message = tr("The manual activation key file you have selected is invalid. Select correct manual activation key file. "
                             "If problem continues, please contact support team.");
                break;
            case QnLicense::InvalidHardwareID:
                message = tr("This license key has been previously activated to hardware id %1. Please contact support team to get a valid license key.")
                    .arg(QString::fromUtf8(license->hardwareId()));
                break;
            case QnLicense::InvalidBrand:
            case QnLicense::InvalidType:
                message = tr("You are trying to activate an incompatible license with your software. Please contact support team to get a valid license key.");
                break;
            default:
                break;
            }

            if (!message.isEmpty())
                emit showMessageLater(tr("License Activation"), message, true);
        }

        
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}
