#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"
#include <utils/common/app_info.h>

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QUrlQuery>

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTreeWidgetItem>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>

#include <client/client_translation_manager.h>

#include <llutil/hardware_id.h>

#include <nx/fusion/serialization/json_functions.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/models/license_list_model.h>
#include <ui/dialogs/license_details_dialog.h>
#include <ui/widgets/common/snapped_scrollbar.h>

#include <utils/license_usage_helper.h>
#include <utils/common/product_features.h>

QnLicenseManagerWidget::QnLicenseManagerWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::LicenseManagerWidget),
    m_model(new QnLicenseListModel(this)),
    m_httpClient(nullptr),
    m_licenses()
{
    ui->setupUi(this);

    QnSnappedScrollBar *tableScrollBar = new QnSnappedScrollBar(this);
    ui->gridLicenses->setVerticalScrollBar(tableScrollBar->proxyScrollBar());

    QSortFilterProxyModel* sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(m_model);

    ui->gridLicenses->setIconSize(QSize(20, 20));
    ui->gridLicenses->setModel(sortModel);
    ui->gridLicenses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->gridLicenses->header()->setSortIndicator(QnLicenseListModel::LicenseKeyColumn, Qt::AscendingOrder);

    setHelpTopic(this, Qn::SystemSettings_Licenses_Help);

    connect(ui->detailsButton,  &QPushButton::clicked, this, &QnLicenseManagerWidget::at_licenseDetailsButton_clicked);
    connect(ui->removeButton,   &QPushButton::clicked, this, &QnLicenseManagerWidget::at_removeButton_clicked);

    connect(ui->gridLicenses->selectionModel(), &QItemSelectionModel::currentChanged,   this, &QnLicenseManagerWidget::updateButtons);
    connect(ui->gridLicenses->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QnLicenseManagerWidget::updateButtons);

    connect(ui->gridLicenses,   &QTreeView::doubleClicked,      this,   &QnLicenseManagerWidget::at_gridLicenses_doubleClicked);
    connect(ui->licenseWidget,  &QnLicenseWidget::stateChanged, this,   &QnLicenseManagerWidget::at_licenseWidget_stateChanged);

    connect(this, &QnLicenseManagerWidget::showMessageLater, this, &QnLicenseManagerWidget::showMessage, Qt::QueuedConnection);

    auto updateLicensesIfNeeded = [this]
    {
        if (!isVisible())
            return;

        updateLicenses();
    };

    QnCamLicenseUsageWatcher* camerasUsageWatcher = new QnCamLicenseUsageWatcher(this);
    QnVideoWallLicenseUsageWatcher* videowallUsageWatcher = new QnVideoWallLicenseUsageWatcher(this);
    connect(camerasUsageWatcher,    &QnLicenseUsageWatcher::licenseUsageChanged,    this,   updateLicensesIfNeeded);
    connect(videowallUsageWatcher,  &QnLicenseUsageWatcher::licenseUsageChanged,    this,   updateLicensesIfNeeded);
    connect(qnLicensePool,          &QnLicensePool::licensesChanged,                this,   updateLicensesIfNeeded);
}

QnLicenseManagerWidget::~QnLicenseManagerWidget()
{}

void QnLicenseManagerWidget::loadDataToUi()
{
    updateLicenses();
}

bool QnLicenseManagerWidget::hasChanges() const
{
    return false;
}

void QnLicenseManagerWidget::applyChanges()
{
    /* This widget is read-only. */
}

void QnLicenseManagerWidget::showEvent(QShowEvent *event)
{
    base_type::showEvent(event);
    updateLicenses();
}

void QnLicenseManagerWidget::updateLicenses()
{
    bool connected = false;
    for (const QnPeerRuntimeInfo& info: QnRuntimeInfoManager::instance()->items()->getItems())
    {
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
    m_model->updateLicenses(m_licenses);

    /* Update info label. */
    bool useRedLabel = false;

    if (!m_licenses.isEmpty())
    {
        // TODO: #Elric #TR total mess with numerous forms, and no idea how to fix it in a sane way

        QString msg(tr("The software is licensed to: "));

        QnCamLicenseUsageHelper camUsageHelper;
        QnVideoWallLicenseUsageHelper vwUsageHelper;
        QList<QnLicenseUsageHelper*> helpers;
        helpers
            << &camUsageHelper
            << &vwUsageHelper
            ;

        for (QnLicenseUsageHelper* helper: helpers)
        {
            for(Qn::LicenseType lt: helper->licenseTypes())
            {
                if (helper->totalLicenses(lt) > 0)
                    msg += L'\n' + lit("%1 %2").arg(helper->totalLicenses(lt)).arg(QnLicense::longDisplayName(lt));
            }
        }

        for (QnLicenseUsageHelper* helper: helpers)
        {
            if (!helper->isValid())
            {
                useRedLabel = true;
                for (Qn::LicenseType lt: helper->licenseTypes())
                {
                    if (helper->usedLicenses(lt) > 0)
                        msg += L'\n' + tr("At least %n %2 are required", "", helper->usedLicenses(lt)).arg(QnLicense::longDisplayName(lt));
                }
            }
            else
            {
                for (Qn::LicenseType lt: helper->licenseTypes())
                {
                    if (helper->usedLicenses(lt) > 0)
                        msg += L'\n' + tr("%n %2 are currently in use", "", helper->usedLicenses(lt)).arg(QnLicense::longDisplayName(lt));
                }
            }
        }
        ui->infoLabel->setText(msg);
    }
    else
    {
        if (qnLicensePool->currentHardwareId().isEmpty())
        {
            ui->infoLabel->setText(tr("Obtaining licenses from server..."));
            useRedLabel = false;
        }
        else
        {
            QString text = (qnProductFeatures().freeLicenseCount > 0) ?
                tr("You do not have a valid license installed.") + L'\n' + tr("Please activate your commercial or trial license.") :
                tr("You do not have a valid license installed.") + L'\n' + tr("Please activate your commercial license.");
            ui->infoLabel->setText(text);
            useRedLabel = true;
        }
    }

    QPalette palette = this->palette();
    if (useRedLabel)
        setWarningStyle(&palette);
    ui->infoLabel->setPalette(palette);

    updateButtons();
}

void QnLicenseManagerWidget::showMessage(const QString &title, const QString &message, bool warning)
{
    QScopedPointer<QnWorkbenchStateDependentDialog<QnMessageBox>> messageBox(new QnWorkbenchStateDependentDialog<QnMessageBox>(this));
    messageBox->setIcon(warning ? QnMessageBox::Warning : QnMessageBox::Information);
    messageBox->setWindowTitle(title);
    messageBox->setText(message);
    QPushButton* copyButton = messageBox->addButton(tr("Copy to Clipboard"), QDialogButtonBox::HelpRole);
    connect(copyButton, &QPushButton::clicked, this, [this, message]
    {
        qApp->clipboard()->setText(message);
    });
    messageBox->setStandardButtons(QDialogButtonBox::Ok);
    messageBox->setEscapeButton(QDialogButtonBox::Ok);
    messageBox->setDefaultButton(QDialogButtonBox::Ok);
    messageBox->exec();
}

void QnLicenseManagerWidget::updateFromServer(const QByteArray &licenseKey, bool infoMode, const QUrl &url)
{
    const QVector<QString> hardwareIds = qnLicensePool->hardwareIds();

    if (QnRuntimeInfoManager::instance()->remoteInfo().isNull())
    {
        emit showMessageLater(tr("License Activation"),
                              tr("Network error has occurred during automatic license activation.") + L'\n' + tr("Try to activate your license manually."),
                              true);
        ui->licenseWidget->setOnline(false);
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }

    if (!m_httpClient)
        m_httpClient = new QNetworkAccessManager(this);

    QNetworkRequest request;
    request.setUrl(url.toString());

    QUrlQuery params;
    params.addQueryItem(lit("license_key"), QLatin1String(licenseKey));

    int n = 1;
    for (const QByteArray& licenseKey: QnLicenseListHelper(m_licenses).allLicenseKeys())
    {
        params.addQueryItem(lit("license_key%1").arg(n), QLatin1String(licenseKey));
        n++;
    }

    for (const QString& hwid: hardwareIds)
    {
        int version = LLUtil::hardwareIdVersion(hwid);

        QString name;
        if (version == 0)
            name = lit("oldhwid[]");
        else if (version == 1)
            name = lit("hwid[]");
        else
            name = lit("hwid%1[]").arg(version);

        params.addQueryItem(name, hwid);
    }

    if (infoMode)
        params.addQueryItem(lit("mode"), lit("info"));

    ec2::ApiRuntimeData runtimeData = QnRuntimeInfoManager::instance()->remoteInfo().data;

    params.addQueryItem(lit("box"), runtimeData.box);
    params.addQueryItem(lit("brand"), runtimeData.brand);
    params.addQueryItem(lit("version"), qnCommon->engineVersion().toString());
    params.addQueryItem(lit("lang"), qnCommon->instance<QnClientTranslationManager>()->getCurrentLanguage());

    if (!runtimeData.nx1mac.isEmpty())
    {
        params.addQueryItem(lit("mac"), runtimeData.nx1mac);
    }

    if (!runtimeData.nx1serial.isEmpty())
    {
        params.addQueryItem(lit("serial"), runtimeData.nx1serial);
    }

    QNetworkReply *reply = m_httpClient->post(request, params.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_downloadError()));
    connect(reply, &QNetworkReply::finished, this, [this, licenseKey, infoMode, url, reply]
    {
        QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).value<QUrl>();
        if (!redirectUrl.isEmpty())
        {
            updateFromServer(licenseKey, infoMode, redirectUrl);    //TODO: #GDM add possible redirect loop check
            return;
        }
        processReply(reply, licenseKey, url, infoMode);
    });
}

void QnLicenseManagerWidget::validateLicenses(const QByteArray& licenseKey, const QList<QnLicensePtr> &licenses)
{
    QList<QnLicensePtr> licensesToUpdate;
    QnLicensePtr keyLicense;

    QnLicenseListHelper licenseListHelper(qnLicensePool->getLicenses());
    for (const QnLicensePtr& license: licenses)
    {
        if (!license)
            continue;

        if (license->key() == licenseKey)
            keyLicense = license;

        QnLicensePtr ownLicense = licenseListHelper.getLicenseByKey(license->key());
        if (!ownLicense || ownLicense->signature() != license->signature())
            licensesToUpdate.append(license);
    }

    if (!licensesToUpdate.isEmpty())
    {
        auto addLisencesHandler = [this, licensesToUpdate, licenseKey](int reqID, ec2::ErrorCode errorCode)
        {
            Q_UNUSED(reqID);
            at_licensesReceived(licenseKey, errorCode, licensesToUpdate);
        };
        int handle = QnAppServerConnectionFactory::getConnection2()->getLicenseManager(Qn::kDefaultUserAccess)->addLicenses(
            licensesToUpdate, this, addLisencesHandler);
    }

    /* There is an issue when we are trying to activate and Edge license on the PC.
     * In this case activation server will return no error but local check will fail.  */
    if (!keyLicense)
    {
        /* QNetworkReply slots should not start event loop. */
        emit showMessageLater(tr("License Activation"),
                              tr("You are trying to activate an incompatible license with your software. "
                                 "Please contact support team to get a valid license key."),
                              true);
    }
    else if (licenseListHelper.getLicenseByKey(licenseKey))
    {
        emit showMessageLater(tr("License Activation"),
                              tr("License has already been activated."),
                              true);
    }
}

void QnLicenseManagerWidget::showLicenseDetails(const QnLicensePtr &license)
{
    if (!license)
        return;

    QScopedPointer<QnLicenseDetailsDialog> dialog(new QnLicenseDetailsDialog(license, this));
    dialog->exec();
}

QnLicenseList QnLicenseManagerWidget::selectedLicenses() const
{
    QnLicenseList result;
    for (const QModelIndex& idx : ui->gridLicenses->selectionModel()->selectedIndexes())
    {
        QnLicensePtr license = idx.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();
        if (license && !result.contains(license))
            result << license;
    }
    return result;
}

bool QnLicenseManagerWidget::canRemoveLicense(const QnLicensePtr &license) const
{
    NX_ASSERT(license);
    if (!license)
        return false;

    QnLicense::ErrorCode errCode = QnLicense::NoError;
    return !license->isValid(&errCode) && errCode != QnLicense::FutureLicense;
}

void QnLicenseManagerWidget::updateButtons()
{
    QModelIndex idx = ui->gridLicenses->selectionModel()->currentIndex();
    QnLicensePtr license = m_model->license(idx);

    ui->detailsButton->setEnabled(!license.isNull());

    QnLicenseList selected = selectedLicenses();
    bool canRemoveAny = std::any_of(selected.cbegin(), selected.cend(), [this](const QnLicensePtr& license)
    {
        return canRemoveLicense(license);
    });

    ui->removeButton->setEnabled(canRemoveAny);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseManagerWidget::at_licensesReceived(const QByteArray& licenseKey, ec2::ErrorCode errorCode, const QnLicenseList& licenses)
{
    QnLicenseListHelper licenseListHelper(licenses);
    QnLicensePtr license = licenseListHelper.getLicenseByKey(licenseKey);

    QString message;
    if (!license || (errorCode != ec2::ErrorCode::ok))
        message = tr("There was a problem activating your license key. A network error has occurred.");
    else if (license)
        message = tr("License was successfully activated.");

    if (!licenses.isEmpty())
    {
        m_licenses.append(licenses);
        qnLicensePool->addLicenses(licenses);
    }

    if (!message.isEmpty())
        QnMessageBox::information(this, tr("License Activation"), message);

    ui->licenseWidget->setSerialKey(QString());

    updateLicenses();
}

void QnLicenseManagerWidget::at_downloadError()
{
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender()))
    {
        disconnect(reply, NULL, this, NULL); //avoid double "onError" handling
        reply->deleteLater();

        /* QNetworkReply slots should not start eventLoop */
        emit showMessageLater(tr("License Activation ") + reply->errorString(),
                              tr("Network error has occurred during automatic license activation. "
                                 "Please contact support to activate your license key manually."),
                              true);

        ui->licenseWidget->setOnline(false);
    }
    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::processReply(QNetworkReply *reply, const QByteArray& licenseKey, const QUrl &url, bool infoMode)
{
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

    while (!is.atEnd())
    {
        QnLicensePtr license = QnLicense::readFromStream(is);
        if (!license)
            break;

        QnLicense::ErrorCode errCode = QnLicense::NoError;

        if (infoMode)
        {
            if (!license->isValid(&errCode, QnLicense::VM_CheckInfo) && errCode != QnLicense::Expired)
            {
                emit showMessageLater(tr("License Activation"), tr("Unable to activate license:  %1").arg(QnLicense::errorMessage(errCode)), true);
                ui->licenseWidget->setState(QnLicenseWidget::Normal);
            }
            else
            {
                updateFromServer(license->key(), false, url);
            }

            return;
        }
        else
        {
            if (license->isValid(&errCode, QnLicense::VM_JustCreated))
                licenses.append(license);
            else if (errCode == QnLicense::Expired)
                licenses.append(license); // ignore expired error code
            else if (errCode == QnLicense::FutureLicense)
                licenses.append(license); // add future licenses
        }
    }

    validateLicenses(licenseKey, licenses);


    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::at_gridLicenses_doubleClicked(const QModelIndex &index)
{
    showLicenseDetails(m_model->license(index));
}

void QnLicenseManagerWidget::at_licenseDetailsButton_clicked()
{
    QModelIndex index = ui->gridLicenses->selectionModel()->currentIndex();
    showLicenseDetails(m_model->license(index));
}

void QnLicenseManagerWidget::at_removeButton_clicked()
{
    for (const QnLicensePtr& license : selectedLicenses())
    {
        if (!canRemoveLicense(license))
            continue;

        auto removeLisencesHandler = [this, license](int reqID, ec2::ErrorCode errorCode)
        {
            at_licenseRemoved(reqID, errorCode, license);
        };

        QnAppServerConnectionFactory::getConnection2()->getLicenseManager(Qn::kDefaultUserAccess)->removeLicense(license, this, removeLisencesHandler);
    }
}

void QnLicenseManagerWidget::at_licenseRemoved(int reqID, ec2::ErrorCode errorCode, QnLicensePtr license)
{
    QN_UNUSED(reqID);
    if (errorCode == ec2::ErrorCode::ok)
    {
        m_model->removeLicense(license);
    }
    else
    {
        showMessage(tr("Remove license"), tr("Unable to remove license from server:  %1").arg(ec2::toString(errorCode)), true);
    }
}

void QnLicenseManagerWidget::at_licenseWidget_stateChanged()
{
    if (ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline())
    {
        updateFromServer(ui->licenseWidget->serialKey().toLatin1(), true, QUrl(QN_LICENSE_URL));
    }
    else
    {
        QnLicensePtr license(new QnLicense(ui->licenseWidget->activationKey()));

        QnLicense::ErrorCode errCode = QnLicense::NoError;
        if (license->isValid(&errCode, QnLicense::VM_JustCreated) || errCode == QnLicense::Expired)
        {
            validateLicenses(license->key(), QList<QnLicensePtr>() << license);
        }
        else
        {
            QString message;
            switch (errCode)
            {
                case QnLicense::InvalidSignature:
                    message = tr("The manual activation key file you have selected is invalid. Select a valid manual activation key file to continue. "
                                 "If problem continues, please contact support team.");
                    break;
                case QnLicense::InvalidHardwareID:
                    message = tr("This license key has been previously activated to hardware id %1. Please contact support team to obtain a valid license key.")
                        .arg(license->hardwareId());
                    break;
                case QnLicense::InvalidBrand:
                case QnLicense::InvalidType:
                    message = tr("You are trying to activate an incompatible license with your software. Please contact support team to get a valid license key.");
                    break;
                case QnLicense::TooManyLicensesPerDevice:
                    message = QnLicense::errorMessage(errCode);
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
