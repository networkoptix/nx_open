#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"
#include <utils/common/app_info.h>

#include <QtCore/QFile>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTextStream>
#include <QtCore/QUrlQuery>

#include <QtGui/QKeyEvent>
#include <QtGui/QClipboard>

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTreeWidgetItem>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <client/client_runtime_settings.h>

#include <licensing/license.h>
#include <licensing/license_validator.h>
#include <licensing/hardware_id_version.h>

#include <nx/fusion/serialization/json_functions.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/models/license_list_model.h>
#include <ui/delegates/license_list_item_delegate.h>
#include <ui/dialogs/license_details_dialog.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <ui/workbench/workbench_context.h>
#include <ui/utils/table_export_helper.h>

#include <utils/license_usage_helper.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>
#include <utils/common/html.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/types/connection_types.h>

#include <nx/vms/client/desktop/license/license_helpers.h>
#include <nx/vms/client/desktop/licensing/license_management_dialogs.h>
#include <nx/vms/client/desktop/ui/dialogs/license_deactivation_reason.h>

#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::common;
using namespace nx::vms::client::desktop;

namespace {

static const QString kHtmlDelimiter = "<br>";
static const QString kEmptyLine = "<br><br>";

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


license::DeactivationErrors filterDeactivationErrors(const license::DeactivationErrors& errors)
{
    license::DeactivationErrors result;
    for (auto it = errors.begin(); it != errors.end(); ++it)
    {
        using ErrorCode = nx::vms::client::desktop::license::Deactivator::ErrorCode;

        const auto code = it.value();
        if (code == ErrorCode::noError || code == ErrorCode::keyIsNotActivated)
            continue; //< Filter out non-actual-error codes

        result.insert(it.key(), it.value());
    }
    return result;
}

QStringList licenseHtmlDescription(const QnLicensePtr& license)
{
    static const auto kLightTextColor = ColorTheme::instance()->color("light10");

    QStringList result;

    const QString licenseKey = html::monospace(
        html::colored(QString::fromLatin1(license->key()), kLightTextColor));

    const QString channelsCount = QnLicenseManagerWidget::tr(
        "%n channels.", "", license->cameraCount());

    result.push_back(licenseKey);
    result.push_back(license->displayName() + ", " + channelsCount);
    return result;
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
    QnWorkbenchContextAware(parent),
    ui(new Ui::LicenseManagerWidget),
    m_model(new QnLicenseListModel(this)),
    m_validator(new QnLicenseValidator(commonModule(), this))
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
    m_exportLicensesButton->setText(tr("Export"));
    m_exportLicensesButton->setFlat(true);
    m_exportLicensesButton->resize(m_exportLicensesButton->minimumSizeHint());

    static const int kButtonTopAdjustment = -4;
    anchorWidgetToParent(
        m_exportLicensesButton, Qt::TopEdge | Qt::RightEdge, {0, kButtonTopAdjustment, 0, 0});

    SnappedScrollBar* tableScrollBar = new SnappedScrollBar(this);
    ui->gridLicenses->setVerticalScrollBar(tableScrollBar->proxyScrollBar());

    QSortFilterProxyModel* sortModel = new QnLicenseListSortProxyModel(this);
    sortModel->setSourceModel(m_model);

    ui->gridLicenses->setModel(sortModel);
    ui->gridLicenses->setItemDelegate(new QnLicenseListItemDelegate(this));

    ui->gridLicenses->header()->setSectionsMovable(false);
    ui->gridLicenses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->gridLicenses->header()->setSectionResizeMode(QnLicenseListModel::ServerColumn, QHeaderView::Stretch);
    ui->gridLicenses->header()->setSectionResizeMode(QnLicenseListModel::LicenseKeyColumn, QHeaderView::ResizeMode::ResizeToContents);
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
                    if (ui->removeButton->isEnabled() && m_isRemoveTakeAwayOperation)
                        takeAwaySelectedLicenses();
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
        this, &QnLicenseManagerWidget::takeAwaySelectedLicenses);

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

    auto camerasUsageWatcher = new QnCamLicenseUsageWatcher(commonModule(), this);
    auto videowallUsageWatcher = new QnVideoWallLicenseUsageWatcher(commonModule(), this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,
        updateLicensesIfNeeded);
    connect(videowallUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,
        updateLicensesIfNeeded);
    connect(commonModule()->licensePool(), &QnLicensePool::licensesChanged, this,
        updateLicensesIfNeeded);

    connect(commonModule(), &QnCommonModule::remoteIdChanged, this,
        [this]() { m_deactivationReason = RequestInfo(); });
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
    for (const QnPeerRuntimeInfo& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.peer.peerType != nx::vms::api::PeerType::server)
            continue;
        connected = true;
        break;
    }

    setEnabled(connected);

    m_licenses = licensePool()->getLicenses();

    QnLicenseListHelper licenseListHelper(m_licenses);

    /* Update license widget. */
    ui->licenseWidget->setHardwareId(licensePool()->currentHardwareId());
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

        QnCamLicenseUsageHelper camUsageHelper(commonModule());
        QnVideoWallLicenseUsageHelper vwUsageHelper(commonModule());
        QList<QnLicenseUsageHelper*> helpers{ &camUsageHelper, &vwUsageHelper };

        for (auto helper: helpers)
        {
            for (Qn::LicenseType lt: helper->licenseTypes())
            {
                const int total = helper->totalLicenses(lt);
                if (total > 0)
                    messages << QnLicense::displayText(lt, total);
            }
        }

        for (auto helper: helpers)
        {
            for (Qn::LicenseType lt: helper->licenseTypes())
            {
                const int used = helper->usedLicenses(lt);
                if (used == 0)
                    continue;

                if (helper->isValid(lt))
                {
                    messages << tr("%1 are currently in use",
                        "Text like '6 Profesional Licenses' will be substituted",
                        used)
                        .arg(QnLicense::displayText(lt, used));
                }
                else
                {
                    const int required = helper->requiredLicenses(lt);
                    messages << setWarningStyleHtml(
                        tr("At least %1 are required",
                            "Text like '6 Profesional Licenses' will be substituted",
                            required)
                        .arg(QnLicense::displayText(lt, required)));
                }
            }
        }

        ui->infoLabel->setText(messages.join("<br/>"));
    }

    updateButtons();
}

void QnLicenseManagerWidget::updateFromServer(
    const QByteArray& licenseKey,
    bool infoMode,
    const QUrl& url)
{
    const QVector<QString> hardwareIds = licensePool()->hardwareIds();

    if (runtimeInfoManager()->remoteInfo().isNull())
    {
        // QNetworkReply slots should not start event loop.
        executeLater([this]{ LicenseActivationDialogs::networkError(this); }, this);

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

    const auto runtimeData = runtimeInfoManager()->remoteInfo().data;

    params.addQueryItem(lit("box"), runtimeData.box);
    params.addQueryItem(lit("brand"), runtimeData.brand);
    params.addQueryItem(lit("version"), commonModule()->engineVersion().toString());
    params.addQueryItem(lit("lang"), qnRuntime->locale());

    if (!runtimeData.nx1mac.isEmpty())
    {
        params.addQueryItem(lit("mac"), runtimeData.nx1mac);
    }

    if (!runtimeData.nx1serial.isEmpty())
    {
        params.addQueryItem(lit("serial"), runtimeData.nx1serial);
    }

    const auto messageBody = params.query(QUrl::FullyEncoded).toUtf8();
    NX_VERBOSE(this, licenseRequestLogString(messageBody, licenseKey));
    QNetworkReply* reply = m_httpClient->post(request, messageBody);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(at_downloadError()));
    connect(reply, &QNetworkReply::finished, this,
        [this, licenseKey, infoMode, url, reply]
        {
            const QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
                .value<QUrl>();
            if (!redirectUrl.isEmpty())
            {
                // TODO: #GDM add possible redirect loop check.
                updateFromServer(licenseKey, infoMode, redirectUrl);
                return;
            }
            processReply(reply, licenseKey, url, infoMode);
        });
}

void QnLicenseManagerWidget::validateLicenses(
    const QByteArray& licenseKey,
    const QnLicenseList& licenses)
{
    QList<QnLicensePtr> licensesToUpdate;
    QnLicensePtr keyLicense;

    QnLicenseListHelper licenseListHelper(licensePool()->getLicenses());
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
        auto addLisencesHandler =
            [this, licensesToUpdate, licenseKey](int /*reqID*/, ec2::ErrorCode errorCode)
            {
                at_licensesReceived(licenseKey, errorCode, licensesToUpdate);
            };
        commonModule()->ec2Connection()->getLicenseManager(Qn::kSystemAccess)->addLicenses(
            licensesToUpdate, this, addLisencesHandler);
    }

    /* There is an issue when we are trying to activate and Edge license on the PC.
     * In this case activation server will return no error but local check will fail.  */
    if (!keyLicense)
    {
        // QNetworkReply slots should not start event loop.
        executeLater([this]{ LicenseActivationDialogs::licenseIsIncompatible(this); }, this);
    }
    else if (licenseListHelper.getLicenseByKey(licenseKey))
    {
        executeLater(
            [this]{ LicenseActivationDialogs::licenseAlreadyActivatedHere(this); }, this);
    }
}

void QnLicenseManagerWidget::showLicenseDetails(const QnLicensePtr &license)
{
    if (!NX_ASSERT(license))
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
    if (!NX_ASSERT(license))
        return false;

    const auto errorCode = m_validator->validate(license);
    return errorCode != QnLicenseErrorCode::NoError
        && errorCode != QnLicenseErrorCode::FutureLicense;
}

bool QnLicenseManagerWidget::canDeactivateLicense(const QnLicensePtr &license) const
{
    if (!NX_ASSERT(license))
        return false;

    // TODO: add more checks according to specs
    const auto errorCode = m_validator->validate(license);
    const bool activeLicense = errorCode == QnLicenseErrorCode::NoError; // Only active licenses

    // TODO: Make a common helper function to sync with QnLicenseDetailsDialog constructor.
    const bool acceptedLicenseType = (license->type() != Qn::LC_Trial) && !license->isSaas();

    const auto serverId = m_validator->serverId(license);
    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    const bool onlineServer = server && server->getStatus() == Qn::Online;

    const bool sameHwId =
        [this, serverId, license]() -> bool
        {
            const auto serverItemExists = commonModule()->runtimeInfoManager()->hasItem(serverId);
            if (!serverItemExists)
                return false;
            const auto info = commonModule()->runtimeInfoManager()->item(serverId);
            return info.data.hardwareIds.contains(license->hardwareId());
        }();

    return activeLicense && onlineServer && sameHwId && acceptedLicenseType;
}

void QnLicenseManagerWidget::removeLicense(const QnLicensePtr& license, ForceRemove force)
{
    if (force == ForceRemove::No && !canRemoveLicense(license))
        return;

    const auto removeLisencesHandler =
        [this, license](int reqID, ec2::ErrorCode errorCode)
        {
            at_licenseRemoved(reqID, errorCode, license);
        };

    const auto manager = commonModule()->ec2Connection()->getLicenseManager(Qn::kSystemAccess);
    manager->removeLicense(license, this, removeLisencesHandler);
}

bool QnLicenseManagerWidget::confirmDeactivation(const QnLicenseList& licenses)
{
    ui::dialogs::LicenseDeactivationReason dialog(m_deactivationReason, parentWidget());
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return false;

    QStringList licenseDetails;
    for (const auto& license: licenses)
    {
        if (!NX_ASSERT(license))
            continue;

        QString deactivationsCount =
            tr("%n deactivations remaining.", "", license->deactivationsCountLeft());

        if (license->deactivationsCountLeft() < 2)
            deactivationsCount = setWarningStyleHtml(deactivationsCount);

        QStringList licenseBlock = licenseHtmlDescription(license);

        licenseBlock.push_back(deactivationsCount);
        licenseDetails.push_back(licenseBlock.join(kHtmlDelimiter));
    }

    QnMessageBox confirmationDialog(QnMessageBoxIcon::Question,
        tr("Deactivate licenses?", "", licenses.size()),
        QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);
    confirmationDialog.setInformativeText(licenseDetails.join(kEmptyLine), false);
    confirmationDialog.setInformativeTextFormat(Qt::RichText);
    confirmationDialog.addButton(lit("Deactivate"),
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (confirmationDialog.exec() == QDialogButtonBox::Cancel)
        return false;

    m_deactivationReason = dialog.info();
    return true;
}

void QnLicenseManagerWidget::showDeactivationErrorsDialog(
    const QnLicenseList& licenses,
    const license::DeactivationErrors& errors)
{
    const license::DeactivationErrors filteredErrors = filterDeactivationErrors(errors);

    const bool totalFail = licenses.size() == filteredErrors.size();
    if (totalFail)
    {
        LicenseDeactivationDialogs::deactivationError(
            this,
            licenses,
            filteredErrors);
    }
    else
    {
        const bool deactivateOther = LicenseDeactivationDialogs::partialDeactivationError(
            this,
            licenses,
            filteredErrors);

        if (deactivateOther)
        {
            QnLicenseList filtered;
            std::copy_if(licenses.begin(), licenses.end(), std::back_inserter(filtered),
                [errors](const QnLicensePtr& license)
                {
                    return !errors.contains(license->key());
                });
            deactivateLicenses(filtered);
        }
    }
}

void QnLicenseManagerWidget::deactivateLicenses(const QnLicenseList& licenses)
{
    using Deactivator = nx::vms::client::desktop::license::Deactivator;
    using Result = Deactivator::Result;

    window()->setEnabled(false);
    const auto restoreEnabledGuard = nx::utils::makeSharedGuard(
        [this]() { window()->setEnabled(true); });

    const auto handler =
        [this, licenses, restoreEnabledGuard]
            (Result result, const license::DeactivationErrors& errors)
        {
            switch (result)
            {
                case Result::DeactivationError:
                    showDeactivationErrorsDialog(licenses, errors);
                    break;

                case Result::UnspecifiedError:
                    LicenseDeactivationDialogs::unexpectedError(this, licenses);
                    break;

                case Result::ConnectionError:
                    LicenseDeactivationDialogs::networkError(this);
                    break;

                case Result::ServerError:
                    LicenseDeactivationDialogs::licensesServerError(this, licenses);
                    break;

                case Result::Success:
                    for (const QnLicensePtr& license: licenses)
                        removeLicense(license, ForceRemove::Yes);
                    LicenseDeactivationDialogs::success(this, licenses);
                    break;
            }
        };

    Deactivator::deactivateAsync(
        QnLicenseServer::deactivateUrl(commonModule()),
        m_deactivationReason, licenses, handler, parentWidget());
}

void QnLicenseManagerWidget::takeAwaySelectedLicenses()
{
    const auto licenses = selectedLicenses();
    if (m_isRemoveTakeAwayOperation)
    {
        for (const QnLicensePtr& license: licenses)
            removeLicense(license, ForceRemove::No);
    }
    else
    {
        QnLicenseList deactivatableLicences;
        for (const auto& license: licenses)
        {
            if (canDeactivateLicense(license))
                deactivatableLicences.push_back(license);
        }

        if (!deactivatableLicences.isEmpty() && confirmDeactivation(deactivatableLicences))
            deactivateLicenses(deactivatableLicences);
    }
}

void QnLicenseManagerWidget::exportLicenses()
{
    QnTableExportHelper::exportToFile(
        ui->gridLicenses->model(),
        QnTableExportHelper::getAllIndexes(ui->gridLicenses->model()),
        this,
        tr("Export licenses to a file"));
}

void QnLicenseManagerWidget::updateButtons()
{
    m_exportLicensesButton->setEnabled(!m_licenses.isEmpty());

    QnLicenseList selected = selectedLicenses();
    ui->detailsButton->setEnabled(selected.size() == 1 && !selected[0].isNull());

    const bool canRemoveAny = std::any_of(selected.cbegin(), selected.cend(),
        [this](const QnLicensePtr& license) { return canRemoveLicense(license); });

    const bool isOwner = context()->user() && context()->user()->isOwner();
    const bool canDeactivateAny = isOwner && std::any_of(selected.cbegin(), selected.cend(),
        [this](const QnLicensePtr& license) { return canDeactivateLicense(license); });

    m_isRemoveTakeAwayOperation = canRemoveAny || !canDeactivateAny;

    // TODO: add check for internet connection
    ui->removeButton->setEnabled(canRemoveAny || canDeactivateAny);
    ui->removeButton->setText(m_isRemoveTakeAwayOperation ? tr("Remove") : tr("Deactivate"));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseManagerWidget::at_licensesReceived(const QByteArray& licenseKey, ec2::ErrorCode errorCode, const QnLicenseList& licenses)
{
    QnLicenseListHelper licenseListHelper(licenses);
    QnLicensePtr license = licenseListHelper.getLicenseByKey(licenseKey);

    if (!license || (errorCode != ec2::ErrorCode::ok))
        LicenseActivationDialogs::networkError(this);
    else if (license)
        LicenseActivationDialogs::success(this);

    if (!licenses.isEmpty())
    {
        m_licenses.append(licenses);
        licensePool()->addLicenses(licenses);
    }

    ui->licenseWidget->setSerialKey(QString());

    updateLicenses();
}

void QnLicenseManagerWidget::at_downloadError()
{
    if (const auto reply = qobject_cast<QNetworkReply*>(sender()))
    {
        reply->disconnect(this); //< Avoid double "onError" handling.
        reply->deleteLater();

        // QNetworkReply slots should not start event loop.
        executeLater([this]{ LicenseActivationDialogs::networkError(this); }, this);

        ui->licenseWidget->setOnline(false);
    }
    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void QnLicenseManagerWidget::processReply(
    QNetworkReply* reply,
    const QByteArray& licenseKey,
    const QUrl& url,
    bool infoMode)
{
    QList<QnLicensePtr> licenses;

    QByteArray replyData = reply->readAll();

    NX_VERBOSE(this, licenseReplyLogString(reply, replyData, licenseKey));

    // TODO: #Elric use JSON mapping here.
    // If we can deserialize JSON it means there is an error.
    QJsonObject errorMessage;
    if (QJson::deserialize(replyData, &errorMessage))
    {
        // QNetworkReply slots should not start event loop.
        executeLater([this, errorMessage] { showActivationErrorMessage(errorMessage); }, this);
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

        if (infoMode)
        {
            QnLicenseErrorCode errCode = m_validator->validate(license,
                QnLicenseValidator::VM_CanActivate);

            if (errCode != QnLicenseErrorCode::NoError && errCode != QnLicenseErrorCode::Expired)
            {
                // QNetworkReply slots should not start event loop.
                executeLater(
                    [this, errCode]
                    {
                        LicenseActivationDialogs::activationError(this, errCode);
                    }, this);

                ui->licenseWidget->setState(QnLicenseWidget::Normal);
            }
            else
            {
                updateFromServer(license->key(), /*infoMode*/ false, url);
            }

            return;
        }
        else
        {
            QnLicenseErrorCode errCode = m_validator->validate(license,
                QnLicenseValidator::VM_JustCreated);

            if (errCode == QnLicenseErrorCode::NoError)
                licenses.append(license);
            else if (errCode == QnLicenseErrorCode::Expired)
                licenses.append(license); // ignore expired error code
            else if (errCode == QnLicenseErrorCode::FutureLicense)
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

void QnLicenseManagerWidget::at_licenseRemoved(int /*reqID*/, ec2::ErrorCode errorCode,
    QnLicensePtr license)
{
    if (errorCode == ec2::ErrorCode::ok)
    {
        m_model->removeLicense(license);
    }
    else
    {
        LicenseDeactivationDialogs::failedToRemoveLicense(this, errorCode);
    }
}

void QnLicenseManagerWidget::at_licenseWidget_stateChanged()
{
    if (ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline())
    {
        updateFromServer(
            ui->licenseWidget->serialKey().toLatin1(), /*infoMode*/ true,
            QnLicenseServer::activateUrl(commonModule()).toQUrl());
    }
    else
    {
        QnLicensePtr license(new QnLicense(ui->licenseWidget->activationKey()));

        QnLicenseErrorCode errCode = m_validator->validate(license, QnLicenseValidator::VM_JustCreated);
        if (errCode == QnLicenseErrorCode::NoError || errCode == QnLicenseErrorCode::Expired)
        {
            validateLicenses(license->key(), QList<QnLicensePtr>() << license);
        }
        else
        {
            QString message;
            switch (errCode)
            {
                case QnLicenseErrorCode::InvalidSignature:
                    // QNetworkReply slots should not start event loop.
                    executeLater(
                        [this] { LicenseActivationDialogs::invalidKeyFile(this); }, this);
                    break;
                case QnLicenseErrorCode::InvalidHardwareID:
                    // QNetworkReply slots should not start event loop.
                    executeLater(
                        [this, hwid = license->hardwareId()]
                        {
                            LicenseActivationDialogs::licenseAlreadyActivated(this, hwid);
                        }, this);
                    break;
                case QnLicenseErrorCode::InvalidBrand:
                case QnLicenseErrorCode::InvalidType:
                    // QNetworkReply slots should not start event loop.
                    executeLater(
                        [this] { LicenseActivationDialogs::licenseIsIncompatible(this); }, this);
                    break;
                case QnLicenseErrorCode::TooManyLicensesPerDevice:
                    // QNetworkReply slots should not start event loop.
                    executeLater(
                        [this, errCode]
                        {
                            LicenseActivationDialogs::activationError(this, errCode);
                        }, this);
                    break;
                default:
                    break;
            }
        }

        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}

void QnLicenseManagerWidget::showActivationErrorMessage(QJsonObject errorMessage)
{
    const auto messageId = errorMessage.value(lit("messageId")).toString();

    if (messageId == "DatabaseError")
    {
        LicenseActivationDialogs::databaseErrorOccurred(this);
    }
    else if (messageId == "InvalidData")
    {
        LicenseActivationDialogs::invalidDataReceived(this);
    }
    else if (messageId == "InvalidKey")
    {
        LicenseActivationDialogs::invalidLicenseKey(this);
    }
    else if (messageId == "InvalidBrand")
    {
        LicenseActivationDialogs::licenseIsIncompatible(this);
    }
    else if (messageId == "AlreadyActivated")
    {
        const auto arguments = errorMessage.value(lit("arguments")).toObject().toVariantMap();
        const auto hwid = arguments.value(lit("hwid")).toString();
        const auto time = arguments.value(lit("time")).toString();
        LicenseActivationDialogs::licenseAlreadyActivated(this, hwid, time);
    }
}
