// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_manager_widget.h"
#include "ui_license_manager_widget.h"

#include <QtCore/QFile>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTextStream>
#include <QtCore/QUrlQuery>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTreeWidgetItem>

#include <api/runtime_info_manager.h>
#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/license/license_helpers.h>
#include <nx/vms/client/desktop/licensing/license_management_dialogs.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/dialogs/license_deactivation_reason.h>
#include <nx/vms/client/desktop/utils/layout_widget_hider.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/license/validator.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_license_manager.h>
#include <ui/delegates/license_list_item_delegate.h>
#include <ui/dialogs/license_details_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/license_list_model.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::license;
namespace html = nx::vms::common::html;

namespace {

static const QString kEmptyLine = QString(html::kLineBreak) + html::kLineBreak;

QnPeerRuntimeInfo remoteInfo(QnRuntimeInfoManager* manager)
{
    auto currentServerId = qnClientCoreModule->networkModule()->currentServerId();
    if (!manager->hasItem(currentServerId))
        return QnPeerRuntimeInfo();

    return manager->item(currentServerId);
}

QString licenseReplyLogString(
    QNetworkReply* reply, const QByteArray& replyBody, const QByteArray& licenseKey)
{
    if (!reply)
        return QString();

    static const QString kReplyLogTemplate =
        "\nReceived response from license server (license key is %1):\n"
            "Response: %2 (%3)\n"
            "Headers:\n%4\n"
            "Body:\n%5\n";

    QStringList headers;
    for (const auto& [header, value]: reply->rawHeaderPairs())
        headers.append(nx::format("%1: %2", header, value));

    return nx::format(kReplyLogTemplate,
        licenseKey, reply->error(), reply->errorString(), headers.join('\n'), replyBody);
}

QString licenseRequestLogString(const QByteArray& body, const QByteArray& licenseKey)
{
    static const QString kRequestLogTemplate =
        "\nSending request to license server (license key is %1).\nBody:\n%2\n";
    return nx::format(kRequestLogTemplate, licenseKey, body);
}


license::DeactivationErrors filterDeactivationErrors(const license::DeactivationErrors& errors)
{
    license::DeactivationErrors result;
    for (const auto& [key, code]: errors.asKeyValueRange())
    {
        using ErrorCode = nx::vms::client::desktop::license::Deactivator::ErrorCode;

        if (code == ErrorCode::noError || code == ErrorCode::keyIsNotActivated)
            continue; //< Filter out non-actual-error codes.

        result.insert(key, code);
    }
    return result;
}

QStringList licenseHtmlDescription(const QnLicensePtr& license)
{
    static const auto kLightTextColor = ColorTheme::instance()->color("light10");

    QStringList result;

    const QString licenseKey = html::monospace(
        html::colored(QLatin1StringView(license->key()), kLightTextColor));

    const QString channelsCount = LicenseManagerWidget::tr(
        "%n channels.", "", license->cameraCount());

    result.push_back(licenseKey);
    result.push_back(license->displayName() + ", " + channelsCount);
    return result;
}

class QnLicenseListSortProxyModel : public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    QnLicenseListSortProxyModel(QObject* parent = nullptr):
        base_type(parent)
    {
    }

protected:
    virtual bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
    {
        if (sourceLeft.column() != sourceRight.column())
            return base_type::lessThan(sourceLeft, sourceRight);

        const auto left = sourceLeft.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();
        const auto right = sourceRight.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();

        if (!left || !right)
            return (left < right);

        switch (sourceLeft.column())
        {
            case QnLicenseListModel::ExpirationDateColumn:
                // Permanent licenses should be the last.
                if (left->neverExpire() != right->neverExpire())
                    return right->neverExpire();

                return left->expirationTime() < right->expirationTime();

            case QnLicenseListModel::CameraCountColumn:
                return left->cameraCount() < right->cameraCount();

            default:
                break;
        }

        return base_type::lessThan(sourceLeft, sourceRight);
    }
};

} // namespace

namespace nx::vms::client::desktop {

LicenseManagerWidget::LicenseManagerWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LicenseManagerWidget),
    m_model(new QnLicenseListModel(this)),
    m_validator(new Validator(systemContext(), this))
{
    ui->setupUi(this);

    const QString alertText = tr("You do not have a valid license installed. "
        "Please activate your commercial or trial license.");
    ui->alertBar->setText(alertText);
    ui->alertBar->setRetainSpaceWhenNotDisplayed(false);
    ui->alertBar->setVisible(false);

    m_exportLicensesButton = new QPushButton(ui->licensesGroupBox);
    m_exportLicensesButton->setText(tr("Export"));
    m_exportLicensesButton->setFlat(true);
    m_exportLicensesButton->resize(m_exportLicensesButton->minimumSizeHint());

    static const int kButtonTopAdjustment = -4;
    anchorWidgetToParent(
        m_exportLicensesButton, Qt::TopEdge | Qt::RightEdge, {0, kButtonTopAdjustment, 0, 0});

    m_removeDetailsButtonsHider = std::make_unique<LayoutWidgetHider>(
        ui->gridLayout, QList<QWidget*>({ui->removeButton, ui->detailsButton}), Qt::AlignRight);

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

    // Remove licenses by the [Delete] key.
    installEventHandler(ui->gridLicenses, QEvent::KeyPress, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Delete || (nx::build_info::isMacOsX() && key == Qt::Key_Backspace))
            {
                if (ui->removeButton->isVisible() && m_isRemoveTakeAwayOperation)
                    takeAwaySelectedLicenses();
            }
        });

    setHelpTopic(this, Qn::Licenses_Help);

    connect(ui->detailsButton,  &QPushButton::clicked, this,
        [this]()
        {
            licenseDetailsRequested(ui->gridLicenses->selectionModel()->currentIndex());
        });

    connect(ui->removeButton, &QPushButton::clicked,
        this, &LicenseManagerWidget::takeAwaySelectedLicenses);

    connect(m_exportLicensesButton, &QPushButton::clicked,
        this, &LicenseManagerWidget::exportLicenses);

    connect(ui->gridLicenses->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &LicenseManagerWidget::updateButtons);

    connect(ui->gridLicenses->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &LicenseManagerWidget::updateButtons);

    connect(ui->gridLicenses, &QTreeView::doubleClicked,
        this,   &LicenseManagerWidget::licenseDetailsRequested);

    connect(ui->licenseWidget, &QnLicenseWidget::stateChanged, this,
        &LicenseManagerWidget::handleWidgetStateChange);

    auto updateLicensesIfNeeded = [this]
    {
        if (!isVisible())
            return;

        updateLicenses();
    };

    auto camerasUsageWatcher = new CamLicenseUsageWatcher(systemContext(), this);
    auto videowallUsageWatcher = new VideoWallLicenseUsageWatcher(systemContext(), this);
    connect(camerasUsageWatcher, &UsageWatcher::licenseUsageChanged, this, updateLicensesIfNeeded);
    connect(
        videowallUsageWatcher, &UsageWatcher::licenseUsageChanged, this, updateLicensesIfNeeded);
    connect(licensePool(), &QnLicensePool::licensesChanged, this, updateLicensesIfNeeded);

    connect(context(),
        &QnWorkbenchContext::userChanged,
        this,
        [this]() { m_deactivationReason = {}; });
}

LicenseManagerWidget::~LicenseManagerWidget()
{
}

void LicenseManagerWidget::loadDataToUi()
{
    if (connection())
        updateLicenses();
}

bool LicenseManagerWidget::hasChanges() const
{
    return false;
}

void LicenseManagerWidget::applyChanges()
{
    // This widget is read-only.
}

void LicenseManagerWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    loadDataToUi();
}

void LicenseManagerWidget::updateLicenses()
{
    bool connected = false;
    for (const QnPeerRuntimeInfo& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.peer.peerType == nx::vms::api::PeerType::server)
        {
            connected = true;
            break;
        }
    }

    setEnabled(connected);

    if (connected)
    {
        m_licenses = licensePool()->getLicenses();
        ui->licenseWidget->setHardwareId(licensePool()->currentHardwareId(serverId()));
    }
    else
    {
        m_licenses.clear();
    }

    ListHelper licenseListHelper(m_licenses);

    // Update license widget.
    ui->licenseWidget->setFreeLicenseAvailable(
        !licenseListHelper.haveLicenseKey(nx::branding::freeLicenseKey().toLatin1()));

    // Update grid.
    m_model->updateLicenses(m_licenses);
    ui->licensesGroupBox->setVisible(!m_licenses.isEmpty());
    ui->alertBar->setVisible(m_licenses.isEmpty());

    // Update info label.
    if (!m_licenses.isEmpty())
    {
        // TODO: #sivanov Problem with numerous forms, and no idea how to fix it in a sane way.

        QStringList messages;

        CamLicenseUsageHelper camUsageHelper(systemContext());
        VideoWallLicenseUsageHelper vwUsageHelper(systemContext());
        QList<UsageHelper*> helpers{&camUsageHelper, &vwUsageHelper};

        for (auto helper: helpers)
        {
            for (Qn::LicenseType type: helper->licenseTypes())
            {
                const int total = helper->totalLicenses(type);
                if (total > 0)
                    messages.append(QnLicense::displayText(type, total));
            }
        }

        for (auto helper: helpers)
        {
            for (Qn::LicenseType type: helper->licenseTypes())
            {
                const int used = helper->usedLicenses(type);
                if (used == 0)
                    continue;

                if (helper->isValid(type))
                {
                    messages.append(
                        tr("%1 are currently in use",
                            "Text like '6 Profesional Licenses' will be substituted",
                            used).arg(QnLicense::displayText(type, used)));
                }
                else
                {
                    const int required = helper->requiredLicenses(type);
                    messages.append(setWarningStyleHtml(
                        tr("At least %1 are required",
                            "Text like '6 Profesional Licenses' will be substituted",
                            required).arg(QnLicense::displayText(type, required))));
                }
            }
        }

        ui->infoLabel->setText(messages.join(html::kLineBreak));
    }

    updateButtons();
}

QnLicensePool* LicenseManagerWidget::licensePool() const
{
    return systemContext()->licensePool();
}

QnUuid LicenseManagerWidget::serverId() const
{
    if (auto connection = this->connection(); NX_ASSERT(connection))
        return connection->moduleInformation().id;

    return QnUuid();
}

void LicenseManagerWidget::updateFromServer(
    const QByteArray& licenseKey,
    bool infoMode,
    const QUrl& url)
{
    const QVector<QString> hardwareIds = licensePool()->hardwareIds(serverId());

    if (remoteInfo(runtimeInfoManager()).isNull())
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
    params.addQueryItem("license_key", QLatin1StringView(licenseKey));

    int n = 1;
    for (const QByteArray& licenseKey: ListHelper(m_licenses).allLicenseKeys())
    {
        params.addQueryItem(nx::format("license_key%1", n), QLatin1StringView(licenseKey));
        n++;
    }

    for (const QString& hwid: hardwareIds)
    {
        int version = licensePool()->hardwareIdVersion(hwid);

        QString name;
        if (version == 0)
            name = "oldhwid[]";
        else if (version == 1)
            name = "hwid[]";
        else
            name = nx::format("hwid%1[]", version);

        params.addQueryItem(name, hwid);
    }

    if (infoMode)
        params.addQueryItem("mode", "info");

    const auto runtimeData = remoteInfo(runtimeInfoManager()).data;

    params.addQueryItem("brand", runtimeData.brand);
    params.addQueryItem("version", appContext()->version().toString());
    params.addQueryItem("lang", qnRuntime->locale());

    if (!runtimeData.nx1mac.isEmpty())
        params.addQueryItem("mac", runtimeData.nx1mac);

    if (!runtimeData.nx1serial.isEmpty())
        params.addQueryItem("serial", runtimeData.nx1serial);

    const auto messageBody = params.query(QUrl::FullyEncoded).toUtf8();
    NX_VERBOSE(this, licenseRequestLogString(messageBody, licenseKey));
    QNetworkReply* reply = m_httpClient->post(request, messageBody);

    connect(reply, &QNetworkReply::errorOccurred, this, &LicenseManagerWidget::handleDownloadError);
    connect(reply, &QNetworkReply::finished, this,
        [this, licenseKey, infoMode, url, reply]
        {
            const QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
                .value<QUrl>();
            if (!redirectUrl.isEmpty())
            {
                NX_VERBOSE(this, "Request redirected to %1", redirectUrl);
                // TODO: #sivanov Add possible redirect loop check.
                updateFromServer(licenseKey, infoMode, redirectUrl);
                return;
            }
            processReply(reply, licenseKey, url, infoMode);
        });
}

void LicenseManagerWidget::validateLicenses(
    const QByteArray& licenseKey, const QnLicenseList& licenses)
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    QList<QnLicensePtr> licensesToUpdate;
    QnLicensePtr keyLicense;

    ListHelper licenseListHelper(licensePool()->getLicenses());
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
        connection->getLicenseManager(Qn::kSystemAccess)->addLicenses(
            licensesToUpdate,
            [this, licensesToUpdate, licenseKey](int /*reqID*/, ec2::ErrorCode errorCode)
            {
                ListHelper licenseListHelper(licensesToUpdate);
                QnLicensePtr license = licenseListHelper.getLicenseByKey(licenseKey);

                if (!license || (errorCode != ec2::ErrorCode::ok))
                    LicenseActivationDialogs::networkError(this);
                else if (license)
                    LicenseActivationDialogs::success(this);

                if (!licensesToUpdate.isEmpty())
                {
                    m_licenses.append(licensesToUpdate);
                    licensePool()->addLicenses(licensesToUpdate);
                }

                ui->licenseWidget->setSerialKey(QString());

                updateLicenses();
            },
            this);
    }

    // There is an issue when we are trying to activate an Edge license on a PC. In this case
    // the activation server will return no error but the local check will fail.
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

void LicenseManagerWidget::showLicenseDetails(const QnLicensePtr &license)
{
    if (!NX_ASSERT(license))
        return;

    QScopedPointer<QnLicenseDetailsDialog> dialog(new QnLicenseDetailsDialog(license, this));
    dialog->exec();
}

QnLicenseList LicenseManagerWidget::selectedLicenses() const
{
    QnLicenseList result;
    for (const QModelIndex& index: ui->gridLicenses->selectionModel()->selectedIndexes())
    {
        const auto license = index.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();
        if (license && !result.contains(license))
            result.append(license);
    }
    return result;
}

bool LicenseManagerWidget::canRemoveLicense(const QnLicensePtr& license) const
{
    if (!NX_ASSERT(license))
        return false;

    const auto errorCode = m_validator->validate(license);
    return errorCode != QnLicenseErrorCode::NoError
        && errorCode != QnLicenseErrorCode::FutureLicense;
}

bool LicenseManagerWidget::canDeactivateLicense(const QnLicensePtr& license) const
{
    if (!NX_ASSERT(license))
        return false;

    if (!license->isDeactivatable())
        return false;

    return license->neverExpire() && m_validator->isValid(license)
        && context()->user() && context()->user()->isOwner();
}

void LicenseManagerWidget::removeLicense(const QnLicensePtr& license, ForceRemove force)
{
    if (force == ForceRemove::No && !canRemoveLicense(license))
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    const auto manager = connection->getLicenseManager(Qn::kSystemAccess);
    manager->removeLicense(license,
        [this, license](int /*requestId*/, ec2::ErrorCode errorCode)
        {
            if (errorCode == ec2::ErrorCode::ok)
            {
                m_model->removeLicense(license);
            }
            else
            {
                LicenseDeactivationDialogs::failedToRemoveLicense(this, errorCode);
            }
        },
        this);
}

bool LicenseManagerWidget::confirmDeactivation(const QnLicenseList& licenses)
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
        licenseDetails.push_back(licenseBlock.join(html::kLineBreak));
    }

    QnMessageBox confirmationDialog(QnMessageBoxIcon::Question,
        tr("Deactivate licenses?", "", licenses.size()),
        QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);
    confirmationDialog.setInformativeText(licenseDetails.join(kEmptyLine), false);
    confirmationDialog.setInformativeTextFormat(Qt::RichText);
    confirmationDialog.addButton(tr("Deactivate"),
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (confirmationDialog.exec() == QDialogButtonBox::Cancel)
        return false;

    m_deactivationReason = dialog.info();
    return true;
}

void LicenseManagerWidget::showDeactivationErrorsDialog(
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

void LicenseManagerWidget::deactivateLicenses(const QnLicenseList& licenses)
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
        LicenseServer::deactivateUrl(systemContext()),
        m_deactivationReason, licenses, handler, parentWidget());
}

void LicenseManagerWidget::takeAwaySelectedLicenses()
{
    const auto licenses = selectedLicenses();
    if (m_isRemoveTakeAwayOperation)
    {
        for (const QnLicensePtr& license: licenses)
            removeLicense(license, ForceRemove::No);
    }
    else
    {
        QnLicenseList deactivatableLicenses;
        for (const auto& license: licenses)
        {
            if (canDeactivateLicense(license))
                deactivatableLicenses.push_back(license);
        }

        if (!deactivatableLicenses.isEmpty() && confirmDeactivation(deactivatableLicenses))
            deactivateLicenses(deactivatableLicenses);
    }
}

void LicenseManagerWidget::exportLicenses()
{
    QnTableExportHelper::exportToFile(
        ui->gridLicenses->model(),
        QnTableExportHelper::getAllIndexes(ui->gridLicenses->model()),
        this,
        tr("Export licenses to a file"));
}

void LicenseManagerWidget::updateButtons()
{
    m_exportLicensesButton->setEnabled(!m_licenses.isEmpty());

    QnLicenseList selected = selectedLicenses();
    m_removeDetailsButtonsHider->setVisible(
        ui->detailsButton, selected.size() == 1 && !selected[0].isNull());

    const bool canRemoveAll = selected.size() > 0
        && std::all_of(
            selected.cbegin(),
            selected.cend(),
            [this](const QnLicensePtr& license) { return canRemoveLicense(license); });
    const bool canDeactivateAny = selected.size() > 0
        && std::any_of(
            selected.cbegin(),
            selected.cend(),
            [this](const QnLicensePtr& license) { return canDeactivateLicense(license); });

    m_isRemoveTakeAwayOperation = canRemoveAll || !canDeactivateAny;

    m_removeDetailsButtonsHider->setVisible(ui->removeButton, canRemoveAll || canDeactivateAny);
    ui->removeButton->setText(m_isRemoveTakeAwayOperation ? tr("Remove") : tr("Deactivate"));
}

//-------------------------------------------------------------------------------------------------
// Handlers

void LicenseManagerWidget::handleDownloadError()
{
    if (const auto reply = qobject_cast<QNetworkReply*>(sender()))
    {
        NX_VERBOSE(this,
            "Network error occured: %1\n%2\nError code: %3",
            reply->error(),
            licenseReplyLogString(reply, reply->readAll(), "no-key"),
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());

        reply->disconnect(this); //< Avoid double "onError" handling.
        reply->deleteLater();

        // QNetworkReply slots should not start event loop.
        if (ui->licenseWidget->isFreeLicenseKey())
        {
            executeLater(
                [this]
                {
                    LicenseActivationDialogs::freeLicenseNetworkError(
                        this, licensePool()->currentHardwareId(serverId()));
                },
                this);
        }
        else
        {
            executeLater([this]{ LicenseActivationDialogs::networkError(this); }, this);
        }

        ui->licenseWidget->setOnline(false);
    }
    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void LicenseManagerWidget::processReply(
    QNetworkReply* reply, const QByteArray& licenseKey, const QUrl& url, bool infoMode)
{
    QList<QnLicensePtr> licenses;

    QByteArray replyData = reply->readAll();

    NX_VERBOSE(this, licenseReplyLogString(reply, replyData, licenseKey));

    // TODO: #sivanov Use JSON mapping here.
    // If we can deserialize JSON it means there is an error.
    QJsonObject errorMessage;
    if (QJson::deserialize(replyData, &errorMessage))
    {
        // QNetworkReply slots should not start event loop.
        executeLater([this, errorMessage] { showActivationErrorMessage(errorMessage); }, this);
        ui->licenseWidget->setState(QnLicenseWidget::Normal);
        return;
    }

    QTextStream stream(&replyData);
    stream.setEncoding(QStringConverter::Utf8);
    reply->deleteLater();

    while (!stream.atEnd())
    {
        QnLicensePtr license = QnLicense::readFromStream(stream);
        if (!license)
            break;

        if (infoMode)
        {
            QnLicenseErrorCode errorCode =
                m_validator->validate(license, Validator::VM_CanActivate);

            if (errorCode != QnLicenseErrorCode::NoError
                && errorCode != QnLicenseErrorCode::Expired)
            {
                // QNetworkReply slots should not start event loop.
                executeLater(
                    [this, errorCode, licenseType = license->type()]
                    {
                        LicenseActivationDialogs::activationError(this, errorCode, licenseType);
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
            QnLicenseErrorCode errorCode =
                m_validator->validate(license, Validator::VM_JustCreated);

            switch (errorCode)
            {
                case QnLicenseErrorCode::NoError:
                    licenses.append(license);
                    break;
                case QnLicenseErrorCode::Expired:
                case QnLicenseErrorCode::TemporaryExpired:
                    licenses.append(license); //< Ignore an expired error code.
                    break;
                case QnLicenseErrorCode::FutureLicense:
                    licenses.append(license); //< Add future licenses.
                    break;
                default:
                    break;
            }
        }
    }

    validateLicenses(licenseKey, licenses);

    ui->licenseWidget->setState(QnLicenseWidget::Normal);
}

void LicenseManagerWidget::licenseDetailsRequested(const QModelIndex& index)
{
    if (index.isValid())
        showLicenseDetails(index.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>());
}

void LicenseManagerWidget::handleWidgetStateChange()
{
    if (ui->licenseWidget->state() != QnLicenseWidget::Waiting)
        return;

    if (ui->licenseWidget->isOnline())
    {
        updateFromServer(
            ui->licenseWidget->serialKey().toLatin1(), /*infoMode*/ true,
            LicenseServer::activateUrl(systemContext()).toQUrl());
    }
    else
    {
        const QnLicensePtr license(new QnLicense(ui->licenseWidget->activationKey()));
        const QnLicenseErrorCode errorCode =
            m_validator->validate(license, Validator::VM_JustCreated);

        switch (errorCode)
        {
            case QnLicenseErrorCode::NoError:
            case QnLicenseErrorCode::Expired:
            case QnLicenseErrorCode::TemporaryExpired:
                ui->licenseWidget->clearManualActivationUserInput();
                validateLicenses(license->key(), {license});
                break;
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
                        },
                        this);
                break;
            case QnLicenseErrorCode::InvalidBrand:
            case QnLicenseErrorCode::InvalidType:
                    // QNetworkReply slots should not start event loop.
                    executeLater(
                        [this] { LicenseActivationDialogs::licenseIsIncompatible(this); }, this);
                break;
            case QnLicenseErrorCode::TooManyLicensesPerSystem:
                // QNetworkReply slots should not start event loop.
                executeLater(
                    [this, errorCode, licenseType = license->type()]
                    {
                        LicenseActivationDialogs::activationError(this, errorCode, licenseType);
                    },
                    this);
                break;
            default:
                break;
        }

        ui->licenseWidget->setState(QnLicenseWidget::Normal);
    }
}

void LicenseManagerWidget::showActivationErrorMessage(QJsonObject errorMessage)
{
    const auto messageId = errorMessage.value("messageId").toString();

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
        const auto arguments = errorMessage.value("arguments").toObject().toVariantMap();
        const auto hwid = arguments.value("hwid").toString();
        const auto time = arguments.value("time").toString();
        LicenseActivationDialogs::licenseAlreadyActivated(this, hwid, time);
    }
}

} // namespace nx::vms::client::desktop
