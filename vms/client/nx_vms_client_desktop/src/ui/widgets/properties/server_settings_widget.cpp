// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_settings_widget.h"
#include "ui_server_settings_widget.h"

#include <functional>

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyledItemDelegate>

#include <api/model/audit/audit_record.h>
#include <api/server_rest_connection.h>
#include <camera/camera_data_manager.h>
#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/rest/result.h>
#include <nx/network/ssl/certificate.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/network/cloud_url_validator.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/ui/server_certificate_viewer.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

static const int PC_SERVER_MAX_CAMERAS = 128;
static const int ARM_SERVER_MAX_CAMERAS = 12;
static const int EDGE_SERVER_MAX_CAMERAS = 1;

static const QString kLinkPattern("<a href='%1' style='text-decoration: none;'>%2</a>");
static const QString kGeneratedCertificateLink = "#gen";
static const QString kUserProvidedCertificateLink = "#usr";
static const QString kMismatchedCertificateLink = "#mis";

std::optional<QString> certificateName(const std::string& pem)
{
    using namespace nx::network::ssl;

    if (pem.empty())
        return {};

    const auto certificate = Certificate::parse(pem);
    if (certificate.empty())
        return {};

    // Take name of the first certificate in chain.
    const auto name = nx::vms::client::core::certificateName(certificate[0].subject());
    return QString::fromStdString(name.value_or("..."));
}

} // namespace


//#define QN_SHOW_ARCHIVE_SPACE_COLUMN

class QnServerSettingsWidget::Private
{
public:
    Private(QnServerSettingsWidget* parent): q(parent) {}

    void cancelRequests()
    {
        if (auto api = q->connectedServerApi())
        {
            for (auto handle: requests)
                api->cancelRequest(handle);

            if (requestHandle != 0)
            {
                api->cancelRequest(requestHandle);
                requestHandle = 0;
            }
        }
        requests.clear();
    }

    void query(const QnUuid& serverId)
    {
        using nx::network::rest::UbjsonResult;

        if (!NX_ASSERT(q))
            return;

        m_data.clear();
        cancelRequests();

        m_result = {};
        m_serverId = serverId;

        auto api = q->connectedServerApi();
        if (!NX_ASSERT(api))
            return;

        auto callback = nx::utils::guarded(q,
            [this](bool success, rest::Handle handle, UbjsonResult result)
            {
                success &= (result.error == UbjsonResult::Result::Error::NoError);
                if (!success)
                    NX_WARNING(this, "Audit log cannot be loaded: %1", result.errorString);

                processData(success, handle, result.deserialized<QnAuditRecordList>());
            });

        static const auto kLookupInterval = std::chrono::seconds{60};
        const auto now = qnSyncTime->value();

        const auto onlineServers = q->resourcePool()->getAllServers(nx::vms::api::ResourceStatus::online);
        for (const QnMediaServerResourcePtr& server: onlineServers)
        {
            auto handle = api->getAuditLogRecords(
                now - kLookupInterval,
                now,
                callback,
                q->thread(),
                server->getId());

            if (handle > 0)
                requests << handle;
        }
    }

    std::string certificate(const QnUuid& serverId) const
    {
        return NX_ASSERT(m_serverId == serverId) ? m_result : "";
    }

private:
    void processData(bool success, int requestNum, const QnAuditRecordList& records)
    {
        if (!requests.contains(requestNum))
            return;

        requests.remove(requestNum);
        if (success)
            m_data << records;

        if (requests.isEmpty())
            evalResult();
    }

    void evalResult()
    {
        int newestRecordTime = 0;

        for (const auto& record: m_data)
        {
            if (record.eventType != Qn::AR_MitmAttack)
                continue;

            QnUuid id(record.extractParam("serverId"));
            if (id != m_serverId)
                continue;

            if (record.createdTimeSec < newestRecordTime)
                continue;

            newestRecordTime = record.createdTimeSec;
            const auto& actualPem = record.extractParam("actualPem");
            m_result = std::string(actualPem.data(), actualPem.size());
        }

        q->showCertificateMismatchBanner(/*dataLoaded*/ true);
    }

public:
    QSet<int> requests;
    rest::Handle requestHandle = 0;

private:
    QPointer<QnServerSettingsWidget> q;

    QnAuditRecordList m_data;

    std::string m_result;
    QnUuid m_serverId;
};

QnServerSettingsWidget::QnServerSettingsWidget(QWidget* parent /* = 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::ServerSettingsWidget)
    , d(new Private(this))
    , m_server()
    , m_tableBottomLabel()
    , m_maxCamerasAdjusted(false)
    , m_rebuildWasCanceled(false)
    , m_initServerName()
{
    ui->setupUi(this);

    ui->alertBar->init({.level = BarDescription::BarLevel::Error, .isOpenExternalLinks = false});
    connect(ui->alertBar, &CommonMessageBar::linkActivated,
        this, &QnServerSettingsWidget::showServerCertificate);

    setWarningStyle(ui->failoverWarningLabel);

    /* Set up context help. */
    setHelpTopic(ui->nameLabel, ui->nameLineEdit, HelpTopic::Id::ServerSettings_General);
    setHelpTopic(ui->nameLabel, ui->maxCamerasSpinBox, HelpTopic::Id::ServerSettings_General);
    setHelpTopic(ui->nameLabel, ui->locationIdSpinBox, HelpTopic::Id::ServerSettings_General);
    setHelpTopic(ui->ipAddressLabel, ui->ipAddressLineEdit,
        HelpTopic::Id::ServerSettings_General);
    setHelpTopic(ui->portLabel, ui->portLineEdit, HelpTopic::Id::ServerSettings_General);
    setHelpTopic(ui->webCamerasDiscoveryCheckBox, HelpTopic::Id::ServerSettings_General);
    setHelpTopic(ui->failoverGroupBox, HelpTopic::Id::ServerSettings_Failover);

    const auto failoverHint = HintButton::createGroupBoxHint(ui->failoverGroupBox);
    // Notice: this hint button uses help topic from the parent class.
    failoverHint->setHintText(
        tr("Servers with failover enabled will automatically take cameras from offline Servers "
           "with the same Location ID."));

    connect(ui->pingButton, &QPushButton::clicked, this, &QnServerSettingsWidget::at_pingButton_clicked);

    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->failoverGroupBox, &QGroupBox::toggled, this, [this](bool checked)
    {
        ui->maxCamerasWidget->setEnabled(checked);
        updateFailoverLabel();
        emit hasChangesChanged();
    });

    connect(ui->maxCamerasSpinBox, QnSpinboxIntValueChanged, this, [this]
    {
        m_maxCamerasAdjusted = true;
        updateFailoverLabel();
        emit hasChangesChanged();
    });

    connect(ui->locationIdSpinBox, QnSpinboxIntValueChanged, this,
        [this]
        {
            updateFailoverLabel();
            emit hasChangesChanged();
        });

    connect(ui->failoverPriorityButton, &QPushButton::clicked, action(action::OpenFailoverPriorityAction), &QAction::trigger);

    connect(ui->webCamerasDiscoveryCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    retranslateUi();
}

QnServerSettingsWidget::~QnServerSettingsWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

QnMediaServerResourcePtr QnServerSettingsWidget::server() const
{
    return m_server;
}

void QnServerSettingsWidget::setServer(const QnMediaServerResourcePtr &server)
{
    if (m_server == server)
        return;

    if (m_server)
    {
        disconnect(m_server.get(), nullptr, this, nullptr);
    }

    m_server = server;

    if (m_server)
    {
        connect(m_server.get(), &QnResource::nameChanged, this, [this](const QnResourcePtr &resource)
        {
            if (ui->nameLineEdit->text() != m_initServerName)   /// Field was changed
                return;

            m_initServerName = resource->getName();
            ui->nameLineEdit->setText(m_initServerName);
        });

        connect(m_server.get(), &QnResource::urlChanged, this, [this](const QnResourcePtr &resource)
        {
            Q_UNUSED(resource);
            updateUrl();
        });

        connect(m_server.get(), &QnMediaServerResource::webCamerasDiscoveryChanged, this,
            &QnServerSettingsWidget::updateWebCamerasDiscoveryEnabled);

        connect(m_server.get(), &QnMediaServerResource::certificateChanged, this,
            &QnServerSettingsWidget::updateCertificatesInfo);
        connect(m_server.get(), &QnMediaServerResource::statusChanged, this,
            &QnServerSettingsWidget::updateCertificatesInfo);
        connect(m_server.get(), &QnMediaServerResource::redundancyChanged, this,
            &QnServerSettingsWidget::updateFailoverSettings);
        connect(m_server.get(), &QnMediaServerResource::maxCamerasChanged, this, [this]()
            {
                updateMaxCamerasSettings();
                m_maxCamerasAdjusted = true;
            });
        connect(m_server.get(), &QnMediaServerResource::locationIdChanged,
            this, &QnServerSettingsWidget::updateLocationIdSettings);
    }

    updateReadOnly();
}

void QnServerSettingsWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;
    setReadOnly(ui->webCamerasDiscoveryCheckBox, readOnly);
    setReadOnly(ui->failoverGroupBox, readOnly);
    setReadOnly(ui->maxCamerasSpinBox, readOnly);
    setReadOnly(ui->locationIdSpinBox, readOnly);
}

void QnServerSettingsWidget::updateReadOnly()
{
    bool readOnly = [&]()
    {
        if (!m_server)
            return true;

        return !systemContext()->accessController()->hasPermissions(
            m_server, Qn::WritePermission | Qn::SavePermission);
    }();

    setReadOnly(readOnly);

    /* Edge servers cannot be renamed. */
    ui->nameLineEdit->setReadOnly(readOnly || QnMediaServerResource::isEdgeServer(m_server));
}

bool QnServerSettingsWidget::hasChanges() const
{
    if (!m_server)
        return false;

    if (isReadOnly())
        return false;

    if (m_server->getName() != ui->nameLineEdit->text())
        return true;

    if (m_server->isRedundancy() != ui->failoverGroupBox->isChecked())
        return true;

    if (m_server->isRedundancy())
    {
        if (m_server->getMaxCameras() != ui->maxCamerasSpinBox->value())
            return true;
        if (m_server->locationId() != ui->locationIdSpinBox->value())
            return true;
    }

    if (isWebCamerasDiscoveryEnabledChanged())
        return true;

    return false;
}

void QnServerSettingsWidget::retranslateUi()
{
    ui->failoverGroupBox->setTitle(tr("Failover"));

    ui->maxCamerasLabel->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Max devices on this server:"),
        tr("Max cameras on this server:")
    ));

    updateFailoverLabel();
}

bool QnServerSettingsWidget::canApplyChanges() const
{
    return !ui->nameLineEdit->text().trimmed().isEmpty();
}

bool QnServerSettingsWidget::isNetworkRequestRunning() const
{
    return !d->requests.empty() || d->requestHandle != 0;
}

void QnServerSettingsWidget::updateMaxCamerasSettings()
{
    if (!m_server)
        return;

    int maxCameras;
    if (m_server->getServerFlags().testFlag(vms::api::SF_Edge))
        maxCameras = EDGE_SERVER_MAX_CAMERAS;   //edge server
    else if (m_server->getServerFlags().testFlag(vms::api::SF_ArmServer))
        maxCameras = ARM_SERVER_MAX_CAMERAS;   //generic ARM based servre
    else
        maxCameras = PC_SERVER_MAX_CAMERAS;    //PC server
    ui->maxCamerasSpinBox->setMaximum(maxCameras);

    int currentMaxCamerasValue = m_server->getMaxCameras();
    if (currentMaxCamerasValue == 0)
        currentMaxCamerasValue = maxCameras;

    ui->maxCamerasSpinBox->setValue(currentMaxCamerasValue);
    ui->maxCamerasWidget->setEnabled(m_server->isRedundancy());
}

void QnServerSettingsWidget::updateLocationIdSettings()
{
    if (!m_server)
        return;

    ui->locationIdSpinBox->setValue(m_server->locationId());
}

void QnServerSettingsWidget::updateFailoverSettings()
{
    if (!m_server)
        return;

    updateMaxCamerasSettings();
    updateLocationIdSettings();

    ui->failoverGroupBox->setChecked(m_server->isRedundancy());
    updateFailoverLabel();
}

void QnServerSettingsWidget::loadDataToUi()
{
    if (!m_server)
        return;

    m_initServerName = m_server->getName();
    ui->nameLineEdit->setText(m_initServerName);

    updateFailoverSettings();
    m_maxCamerasAdjusted = false;
    updateUrl();
    updateWebCamerasDiscoveryEnabled();
    updateCertificatesInfo();
}

void QnServerSettingsWidget::applyChanges()
{
    if (!m_server)
        return;

    if (isReadOnly())
        return;

    m_server->setName(ui->nameLineEdit->text());
    m_server->setMaxCameras(ui->maxCamerasSpinBox->value());
    m_server->setLocationId(ui->locationIdSpinBox->value());
    m_server->setRedundancy(ui->failoverGroupBox->isChecked());
    m_server->setWebCamerasDiscoveryEnabled(isWebCamerasDiscoveryEnabled());

    auto callback = nx::utils::guarded(this,
        [this](bool /*success*/, rest::Handle requestId)
        {
            if (NX_ASSERT(requestId == d->requestHandle || d->requestHandle == 0))
                d->requestHandle = 0;
        });

    d->requestHandle = qnResourcesChangesManager->saveServer(m_server, callback);
}

void QnServerSettingsWidget::discardChanges()
{
    d->cancelRequests();
}

void QnServerSettingsWidget::updateUrl()
{
    if (!m_server)
        return;

    auto displayInfo = QnResourceDisplayInfo(m_server);
    ui->ipAddressLineEdit->setText(displayInfo.host());
    ui->portLineEdit->setText(QString::number(displayInfo.port()));

    ui->pingButton->setVisible(!isCloudServer(m_server));
}

void QnServerSettingsWidget::updateWebCamerasDiscoveryEnabled()
{
    ui->webCamerasDiscoveryCheckBox->setCheckState(currentIsWebCamerasDiscoveryEnabled()
        ? Qt::CheckState::Checked
        : Qt::CheckState::Unchecked);
}

void QnServerSettingsWidget::updateCertificatesInfo()
{
    static const auto kDashes = QString(4, nx::UnicodeChars::kEnDash);

    int certificateCount = updateCertificatesLabels();

    ui->certificateLabel->setText(tr("Certificates", "", certificateCount));
    ui->certificateHint->setHintText(
        tr("Server utilizes these %n SSL certificates to authenticate its identity",
            "",
            certificateCount));

    // Create an empty link with dash pattern.
    if (!certificateCount)
        ui->certificateLayout->addRow(new QLabel(kDashes, this));

    // Show mismatch banner.
    if (certificateCount && isCertificateMismatch())
    {
        showCertificateMismatchBanner(/*dataLoaded*/ false);
        d->query(m_server->getId());
    }
    else
    {
        ui->alertBar->hide();
    }
}

void QnServerSettingsWidget::showCertificateMismatchBanner(bool dataLoaded)
{
    const auto kBase =
        tr("The certificate received from the Server does not match the pinned certificate.");

    ui->alertBar->setText(dataLoaded
        ? kBase + " " + nx::vms::common::html::localLink(tr("Details"), kMismatchedCertificateLink)
        : kBase);

    // Fixup link color.
    ui->alertBar->hide();

    ui->alertBar->show();
}

int QnServerSettingsWidget::updateCertificatesLabels()
{
    // Clear certificates layout.
    while (ui->certificateLayout->count() > 0)
    {
        auto item = ui->certificateLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    if (!m_server)
        return 0;

    int certificateCount = 0;

    if (auto userProvidedCertificateName = certificateName(m_server->userProvidedCertificate()))
    {
        ++certificateCount;
        QLabel* certificateLabel = new QLabel(
            kLinkPattern.arg(kUserProvidedCertificateLink, *userProvidedCertificateName), this);
        certificateLabel->setContextMenuPolicy(Qt::ContextMenuPolicy::NoContextMenu);
        connect(certificateLabel, &QLabel::linkActivated,
            this, &QnServerSettingsWidget::showServerCertificate);

        ui->certificateLayout->addRow(certificateLabel);
    }

    if (auto generatedCertificateName = certificateName(m_server->certificate()))
    {
        ++certificateCount;
        QLabel* certificateLabel = new QLabel(
            kLinkPattern.arg(kGeneratedCertificateLink, *generatedCertificateName), this);
        certificateLabel->setContextMenuPolicy(Qt::ContextMenuPolicy::NoContextMenu);
        connect(certificateLabel, &QLabel::linkActivated,
            this, &QnServerSettingsWidget::showServerCertificate);

        QLabel* warningImage = nullptr;
        if (isCertificateMismatch())
        {
            warningImage = new QLabel(this);
            warningImage->setPixmap(qnSkin->pixmap("tree/warning_yellow.svg"));
        }

        ui->certificateLayout->addRow(certificateLabel, warningImage);
    }

    return certificateCount;
}

bool QnServerSettingsWidget::isCertificateMismatch() const
{
    return m_server
        && (m_server->getStatus() == nx::vms::api::ResourceStatus::mismatchedCertificate);
}

bool QnServerSettingsWidget::isWebCamerasDiscoveryEnabled() const
{
    return ui->webCamerasDiscoveryCheckBox->isChecked();
}

bool QnServerSettingsWidget::isWebCamerasDiscoveryEnabledChanged() const
{
    return isWebCamerasDiscoveryEnabled() != currentIsWebCamerasDiscoveryEnabled();
}

bool QnServerSettingsWidget::currentIsWebCamerasDiscoveryEnabled() const
{
    return m_server->isWebCamerasDiscoveryEnabled();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnServerSettingsWidget::updateFailoverLabel()
{
    auto getErrorText = [this]
    {
        if (!m_server)
            return QString();

        if (resourcePool()->getResources<QnMediaServerResource>().size() < 2)
            return tr("At least two servers are required for this feature.");

        if (resourcePool()->getAllCameras(m_server, true).size() > ui->maxCamerasSpinBox->value())
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("This server already has more than max devices"),
                tr("This server already has more than max cameras")
            );

        if (!m_server->isRedundancy() && !m_maxCamerasAdjusted)
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("To avoid issues adjust max number of devices"),
                tr("To avoid issues adjust max number of cameras")
            );

        return QString();
    };

    QString error;
    if (ui->failoverGroupBox->isChecked())
        error = getErrorText();

    ui->failoverWarningLabel->setText(error);
    ui->failoverWarningLabel->setHidden(error.isEmpty());
}


void QnServerSettingsWidget::at_pingButton_clicked()
{
    if (!m_server)
        return;

    /* We must always ping the same address that is displayed in the visible field. */
    menu()->trigger(action::PingAction, {Qn::TextRole, ui->ipAddressLineEdit->text()});
}

void QnServerSettingsWidget::showServerCertificate(const QString& id)
{
    using namespace nx::network::ssl;

    if (!NX_ASSERT(m_server))
        return;

    std::string pem;
    ServerCertificateViewer::Mode mode;
    if (id == kUserProvidedCertificateLink)
    {
        pem = m_server->userProvidedCertificate();
        mode = ServerCertificateViewer::Mode::custom;
    }
    else if (id == kGeneratedCertificateLink)
    {
        pem = m_server->certificate();
        mode = ServerCertificateViewer::Mode::pinned;
    }
    else if (id == kMismatchedCertificateLink)
    {
        pem = d->certificate(m_server->getId());
        mode = ServerCertificateViewer::Mode::mismatch;
    }

    if (!NX_ASSERT(!pem.empty()))
        return;

    const auto certificate = Certificate::parse(pem);
    if (!NX_ASSERT(!certificate.empty()))
        return;

    const auto target = m_server->getModuleInformation();
    NX_ASSERT(target.systemName == globalSettings()->systemName());

    auto viewer = new ServerCertificateViewer(m_server, certificate, mode, systemContext(), this);

    viewer->setWindowModality(Qt::ApplicationModal);
    viewer->show();
}
