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

#include <ui/common/widget_anchor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/models/license_list_model.h>
#include <ui/delegates/license_list_item_delegate.h>
#include <ui/dialogs/license_details_dialog.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/utils/table_export_helper.h>

#include <utils/license_usage_helper.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>
#include <nx/utils/log/log.h>

namespace {

QString licenseReplyLogString(
    QNetworkReply* reply,
    const QByteArray& replyBody,
    const QByteArray& licenseKey)
{
    if (!reply)
        return QString();

    static const auto kReplyLogTemplate =
        lit("\nReceived response from license server (license key is %1):\n"
            "Response: %2 (%3)\n"
            "Headers:\n%4\n"
            "Body:\n%5\n");

    QStringList headers;
    for (const auto header: reply->rawHeaderPairs())
    {
        headers.push_back(lit("%1: %2").arg(
            QString::fromLatin1(header.first),
            QString::fromLatin1(header.second)));
    }

    return kReplyLogTemplate.arg(
        QString::fromLatin1(licenseKey),
        QString::number(reply->error()), reply->errorString(),
        headers.join(lit("\n")),
        QString::fromLatin1(replyBody));
}

QString licenseRequestLogString(const QByteArray& body, const QByteArray& licenseKey)
{
    static const auto kRequestLogTemplate =
        lit("\nSending request to license server (license key is %1).\nBody:\n%2\n");
    return kRequestLogTemplate.arg(
        QString::fromLatin1(licenseKey), QString::fromUtf8(body));
}

class QnLicenseListSortProxyModel : public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    QnLicenseListSortProxyModel(QObject* parent = nullptr) :
        base_type(parent)
    {
    }

protected:
    virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
    {
        if (source_left.column() != source_right.column())
            return base_type::lessThan(source_left, source_right);

        QnLicensePtr left = source_left.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();
        QnLicensePtr right = source_right.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();

        if (!left || !right)
            return (left < right);

        switch (source_left.column())
        {
            case QnLicenseListModel::ExpirationDateColumn:
                /* Permanent licenses should be the last. */
                if (left->neverExpire() != right->neverExpire())
                    return right->neverExpire();

                return left->expirationTime() < right->expirationTime();

            case QnLicenseListModel::CameraCountColumn:
                return left->cameraCount() < right->cameraCount();

            default:
                break;
        }

        return base_type::lessThan(source_left, source_right);
    }
};

} // namespace


QnLicenseManagerWidget::QnLicenseManagerWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::LicenseManagerWidget),
    m_model(new QnLicenseListModel(this)),
    m_httpClient(nullptr),
    m_exportLicensesButton(nullptr),
    m_licenses()
{
    ui->setupUi(this);

    QString alertText = tr("You do not have a valid license installed.") + L' ';
    alertText += QnAppInfo::freeLicenseCount() > 0
        ? tr("Please activate your commercial or trial license.")
        : tr("Please activate your commercial license.");
    ui->alertBar->setText(alertText);
    ui->alertBar->setReservedSpace(false);
    ui->alertBar->setVisible(false);

    m_exportLicensesButton = new QPushButton(ui->licensesGroupBox);
    auto anchor = new QnWidgetAnchor(m_exportLicensesButton);
    anchor->setEdges(Qt::TopEdge | Qt::RightEdge);
    static const int kButtonTopAdjustment = -4;
    anchor->setMargins(0, kButtonTopAdjustment, 0, 0);
    m_exportLicensesButton->setText(tr("Export"));
    m_exportLicensesButton->setFlat(true);
    m_exportLicensesButton->resize(m_exportLicensesButton->minimumSizeHint());

    QnSnappedScrollBar* tableScrollBar = new QnSnappedScrollBar(this);
    ui->gridLicenses->setVerticalScrollBar(tableScrollBar->proxyScrollBar());

    QSortFilterProxyModel* sortModel = new QnLicenseListSortProxyModel(this);
    sortModel->setSourceModel(m_model);

    ui->gridLicenses->setModel(sortModel);
    ui->gridLicenses->setItemDelegate(new QnLicenseListItemDelegate(this));

    ui->gridLicenses->header()->setSectionsMovable(false);
    ui->gridLicenses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->gridLicenses->header()->setSectionResizeMode(QnLicenseListModel::ServerColumn, QHeaderView::Stretch);
    ui->gridLicenses->header()->setSortIndicator(QnLicenseListModel::LicenseKeyColumn, Qt::AscendingOrder);

    /* By [Delete] key remove licenses. */
    installEventHandler(ui->gridLicenses, QEvent::KeyPress, this,
        [this](QObject* object, QEvent* event)
        {
            Q_UNUSED(object);
            int key = static_cast<QKeyEvent*>(event)->key();
            switch (key)
            {
                case Qt::Key_Delete:
#if defined(Q_OS_MAC)
                case Qt::Key_Backspace:
#endif
                    removeSelectedLicenses();
                default:
                    return;
            }
        });

    setHelpTopic(this, Qn::SystemSettings_Licenses_Help);

    connect(ui->detailsButton,  &QPushButton::clicked, this,
        [this]()
        {
            licenseDetailsRequested(ui->gridLicenses->selectionModel()->currentIndex());
        });

    connect(ui->removeButton, &QPushButton::clicked,
        this, &QnLicenseManagerWidget::removeSelectedLicenses);

    connect(m_exportLicensesButton, &QPushButton::clicked,
        this, &QnLicenseManagerWidget::exportLicenses);

    connect(ui->gridLicenses->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &QnLicenseManagerWidget::updateButtons);

    connect(ui->gridLicenses->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &QnLicenseManagerWidget::updateButtons);

    connect(ui->gridLicenses, &QTreeView::doubleClicked,
        this,   &QnLicenseManagerWidget::licenseDetailsRequested);

    connect(ui->licenseWidget, &QnLicenseWidget::stateChanged, this,
        &QnLicenseManagerWidget::at_licenseWidget_stateChanged);

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
{
}

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
    ui->licenseWidget->setFreeLicenseAvailable(
        !licenseListHelper.haveLicenseKey(QnAppInfo::freeLicenseKey().toLatin1())
        && (QnAppInfo::freeLicenseCount() > 0));

    /* Update grid. */
    m_model->updateLicenses(m_licenses);
    ui->licensesGroupBox->setVisible(!m_licenses.isEmpty());
    ui->alertBar->setVisible(m_licenses.isEmpty());

    /* Update info label. */
    if (!m_licenses.isEmpty())
    {
        // TODO: #Elric #TR total mess with numerous forms, and no idea how to fix it in a sane way

        QStringList messages;

        QnCamLicenseUsageHelper camUsageHelper;
        QnVideoWallLicenseUsageHelper vwUsageHelper;
        QList<QnLicenseUsageHelper*> helpers{ &camUsageHelper, &vwUsageHelper };

        for (auto helper: helpers)
        {
            for(Qn::LicenseType lt: helper->licenseTypes())
            {
                if (helper->totalLicenses(lt) > 0)
                    messages << lit("%1 %2").arg(helper->totalLicenses(lt)).arg(QnLicense::longDisplayName(lt));
            }
        }

        for (auto helper: helpers)
        {
            for (Qn::LicenseType lt: helper->licenseTypes())
            {
                if (helper->usedLicenses(lt) == 0)
                    continue;

                if (helper->isValid(lt))
                {
                    messages << tr("%n %1 are currently in use", "", helper->usedLicenses(lt))
                        .arg(QnLicense::longDisplayName(lt));
                }
                else
                {
                    messages << setWarningStyleHtml(tr("At least %n %1 are required", "",
                        helper->requiredLicenses(lt)).arg(QnLicense::longDisplayName(lt)));
                }


            }
        }
        ui->infoLabel->setText(messages.join(lit("<br/>")));
    }

    updateButtons();
}

void QnLicenseManagerWidget::showMessageLater(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras,
    CopyToClipboardButton button)
{
    const auto showThisMessage =
        [this, icon, text, extras, button]()
        {
            showMessage(icon, text, extras, button);
        };

    executeDelayedParented(showThisMessage, 0, this);
}

void QnLicenseManagerWidget::showMessage(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras,
    CopyToClipboardButton button)
{
    QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
    messageBox->setIcon(icon);
    messageBox->setText(text);
    messageBox->setInformativeText(extras);

    if (button == CopyToClipboardButton::Show)
    {
        const auto button = messageBox->addButton(
            tr("Copy To Clipboard"), QDialogButtonBox::HelpRole);
        connect(button, &QPushButton::clicked, this,
            [this, extras] { qApp->clipboard()->setText(extras); });
    }

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
        showMessageLater(QnMessageBoxIcon::Critical,
            networkErrorText(), networkErrorExtras(), CopyToClipboardButton::Hide);

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

    const auto messageBody = params.query(QUrl::FullyEncoded).toUtf8();
    NX_LOGX(licenseRequestLogString(messageBody, licenseKey), cl_logINFO);
    QNetworkReply *reply = m_httpClient->post(request, messageBody);

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

void QnLicenseManagerWidget::showIncompatibleLicenceMessageLater()
{
    showMessageLater(QnMessageBoxIcon::Warning,
        tr("Incompatible license"),
        tr("License you are trying to activate is incompatible with your software.")
            + L'\n' + tr("Please contact Customer Support to get a valid license key."),
        CopyToClipboardButton::Hide);
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
        QnAppServerConnectionFactory::getConnection2()->getLicenseManager(Qn::kSystemAccess)->addLicenses(
            licensesToUpdate, this, addLisencesHandler);
    }

    /* There is an issue when we are trying to activate and Edge license on the PC.
     * In this case activation server will return no error but local check will fail.  */
    if (!keyLicense)
    {
        /* QNetworkReply slots should not start event loop. */
        showIncompatibleLicenceMessageLater();
    }
    else if (licenseListHelper.getLicenseByKey(licenseKey))
    {
        showMessageLater(QnMessageBoxIcon::Information,
            tr("You already activated this license"), QString(),
            CopyToClipboardButton::Hide);
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

void QnLicenseManagerWidget::removeSelectedLicenses()
{
    for (const QnLicensePtr& license : selectedLicenses())
    {
        if (!canRemoveLicense(license))
            continue;

        auto removeLisencesHandler = [this, license](int reqID, ec2::ErrorCode errorCode)
        {
            at_licenseRemoved(reqID, errorCode, license);
        };

        QnAppServerConnectionFactory::getConnection2()->getLicenseManager(Qn::kSystemAccess)->removeLicense(license, this, removeLisencesHandler);
    }
}

void QnLicenseManagerWidget::exportLicenses()
{
    QnTableExportHelper::exportToFile(ui->gridLicenses, false, this, tr("Export licenses to a file"));
}

void QnLicenseManagerWidget::updateButtons()
{
    m_exportLicensesButton->setEnabled(!m_licenses.isEmpty());

    QnLicenseList selected = selectedLicenses();
    ui->detailsButton->setEnabled(selected.size() == 1 && !selected[0].isNull());

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

    if (!license || (errorCode != ec2::ErrorCode::ok))
        QnMessageBox::critical(this, networkErrorText(), networkErrorExtras());
    else if (license)
        QnMessageBox::success(this, tr("License activated"));

    if (!licenses.isEmpty())
    {
        m_licenses.append(licenses);
        qnLicensePool->addLicenses(licenses);
    }

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
        showMessageLater(QnMessageBoxIcon::Critical,
            networkErrorText(), networkErrorExtras(),
            CopyToClipboardButton::Hide);

        ui->licenseWidget->setOnline(false);
    }
    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::processReply(QNetworkReply *reply, const QByteArray& licenseKey, const QUrl &url, bool infoMode)
{
    QList<QnLicensePtr> licenses;

    QByteArray replyData = reply->readAll();

    NX_LOGX(licenseReplyLogString(reply, replyData, licenseKey), cl_logINFO);

    // TODO: #Elric use JSON mapping here.
    // If we can deserialize JSON it means there is an error.
    QJsonObject errorMessage;
    if (QJson::deserialize(replyData, &errorMessage))
    {
        /* QNetworkReply slots should not start eventLoop */
        showActivationMessageLater(errorMessage);
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
                showFailedToActivateLicenseLater(QnLicense::errorMessage(errCode));
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

void QnLicenseManagerWidget::licenseDetailsRequested(const QModelIndex& index)
{
    if (index.isValid())
        showLicenseDetails(index.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>());
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
        showMessage(QnMessageBoxIcon::Critical,
            tr("Failed to remove license from Server"), ec2::toString(errorCode),
            CopyToClipboardButton::Hide);
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
                    showMessageLater(QnMessageBoxIcon::Warning,
                        tr("Invalid activation key file"),
                        tr("Select a valid activation key file to continue.")
                            + L'\n' +  getProblemPersistMessage(),
                        CopyToClipboardButton::Hide);
                    break;
                case QnLicense::InvalidHardwareID:
                    showAlreadyActivatedLater(license->hardwareId());
                    break;
                case QnLicense::InvalidBrand:
                case QnLicense::InvalidType:
                    showIncompatibleLicenceMessageLater();
                    break;
                case QnLicense::TooManyLicensesPerDevice:
                    showMessageLater(QnMessageBoxIcon::Warning,
                        tr("This device accepts single channel license only"),
                        getContactSupportMessage(), CopyToClipboardButton::Hide);
                    break;
                default:
                    break;
            }
        }


        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}

void QnLicenseManagerWidget::showActivationMessageLater(const QJsonObject& errorMessage)
{
    const auto messageId = errorMessage.value(lit("messageId")).toString();

    if (messageId == lit("DatabaseError"))
    {
        showFailedToActivateLicenseLater(tr("Database error occurred."));
    }
    else if (messageId == lit("InvalidData"))
    {
        showFailedToActivateLicenseLater(
            tr("Invalid data received. Please contact Customer Support to report the issue."));
    }
    else if (messageId == lit("InvalidKey"))
    {
        showMessageLater(QnMessageBoxIcon::Warning,
            tr("Invalid license key"),
            tr("Please make sure it is entered correctly.")
            + L'\n' + getProblemPersistMessage(),
            CopyToClipboardButton::Hide);
    }
    else if (messageId == lit("InvalidBrand"))
    {
        showIncompatibleLicenceMessageLater();
    }
    else if (messageId == lit("AlreadyActivated"))
    {
        const auto arguments = errorMessage.value(lit("arguments")).toObject().toVariantMap();
        const auto hwid = arguments.value(lit("hwid")).toString();
        const auto time = arguments.value(lit("time")).toString();
        showAlreadyActivatedLater(hwid, time);
    }
}

void QnLicenseManagerWidget::showFailedToActivateLicenseLater(const QString& extras)
{
    showMessageLater(QnMessageBoxIcon::Critical,
        tr("Failed to activate license"), extras,
        CopyToClipboardButton::Hide);
}

QString QnLicenseManagerWidget::getContactSupportMessage()
{
    return tr("Please contact Customer Support to obtain a valid license key.");
}

QString QnLicenseManagerWidget::networkErrorText()
{
    return tr("Network error");
}

QString QnLicenseManagerWidget::networkErrorExtras()
{
    return tr("Please contact Customer Support to activate license key manually.");
}

QString QnLicenseManagerWidget::getProblemPersistMessage()
{
    return tr("If the problem persists, please contact Customer Support.");
}

void QnLicenseManagerWidget::showAlreadyActivatedLater(
    const QString& hwid,
    const QString& time)
{

    //TODO: #GDM #tr almost the same as in QnLicenseUsageHelper::activationMessage
    auto extras = (time.isEmpty()
        ? tr("This license is already activated and linked to hardware ID %1").arg(hwid)
        : tr("This license is already activated and linked to hardware ID %1 on %2")
            .arg(hwid).arg(time));

    extras += L'\n' + getContactSupportMessage();
    showMessageLater(QnMessageBoxIcon::Warning,
        tr("License already activated on another server"),
        extras, CopyToClipboardButton::Show);
}
