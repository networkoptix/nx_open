#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <core/resource/media_server_resource.h>

#include <network/cloud_url_validator.h>

#include <nx/network/http/http_types.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/properties/server_settings_widget.h>
#include <ui/widgets/properties/storage_analytics_widget.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include "ui/widgets/properties/storage_config_widget.h"
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <core/resource/fake_media_server.h>
#include <utils/common/html.h>

using namespace nx::client::desktop::ui;

QnServerSettingsDialog::QnServerSettingsDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(),
    m_generalPage(new QnServerSettingsWidget(this)),
    m_statisticsPage(new QnStorageAnalyticsWidget(this)),
    m_storagesPage(new QnStorageConfigWidget(this)),
    m_webPageLink(new QLabel(this))
{
    ui->setupUi(this);
    setTabWidget(ui->tabWidget);

    addPage(SettingsPage, m_generalPage, tr("General"));
    addPage(StorageManagmentPage, m_storagesPage, tr("Storage Management"));
    addPage(StatisticsPage, m_statisticsPage, tr("Storage Analytics"));

    setupShowWebServerLink();

    //TODO: #GDM #access connect to resource pool to check if server was deleted
    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this,
        [this](const QnResourceList& resources)
        {
            if (isHidden())
                return;

            auto servers = resources.filtered<QnMediaServerResource>(
                [](const QnMediaServerResourcePtr &server)
                {
                    return !server.dynamicCast<QnFakeMediaServerResource>();
                });

            if (!servers.isEmpty())
                setServer(servers.first());
        });

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);
}

QnServerSettingsDialog::~QnServerSettingsDialog()
{
}

void QnServerSettingsDialog::setupShowWebServerLink()
{
    const auto buttonsLayout = qobject_cast<QHBoxLayout*>(ui->buttonBox->layout());
    if (!buttonsLayout)
    {
        NX_ASSERT(false, "Invalid QDialogButtonBox layout");
        return;
    }

    m_webPageLink->setText(makeHref(tr("Server Web Page"), lit("#")));
    buttonsLayout->insertWidget(0, m_webPageLink);
    setHelpTopic(m_webPageLink, Qn::ServerSettings_WebClient_Help);
    connect(m_webPageLink, &QLabel::linkActivated, this,
        [this] { menu()->trigger(action::WebAdminAction, m_server); });
}


QnMediaServerResourcePtr QnServerSettingsDialog::server() const
{
    return m_server;
}

void QnServerSettingsDialog::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    if (!tryClose(false))
        return;

    m_generalPage->setServer(server);
    m_statisticsPage->setServer(server);
    m_storagesPage->setServer(server);

    if (m_server)
        disconnect(m_server, nullptr, this, nullptr);

    m_server = server;

    if (m_server)
        connect(m_server, &QnResource::statusChanged, this, &QnServerSettingsDialog::updateWebPageLink);

    loadDataToUi();
    updateWebPageLink();
}

void QnServerSettingsDialog::retranslateUi()
{
    base_type::retranslateUi();
    if (m_server)
    {
        bool readOnly = !accessController()->hasPermissions(m_server, Qn::WritePermission | Qn::SavePermission);
        setWindowTitle(readOnly
            ? tr("Server Settings - %1 (readonly)").arg(m_server->getName())
            : tr("Server Settings - %1").arg(m_server->getName()));

    }
    else
    {
        setWindowTitle(tr("Server Settings"));
    }
}

void QnServerSettingsDialog::showEvent(QShowEvent* event)
{
    loadDataToUi();
    base_type::showEvent(event);
}

QDialogButtonBox::StandardButton QnServerSettingsDialog::showConfirmationDialog()
{
    NX_ASSERT(m_server, Q_FUNC_INFO, "Server must exist here");

    const auto result = QnMessageBox::question(this,
        tr("Apply changes before switching to another server?"),
        QString(),
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply);

    if (result == QDialogButtonBox::Apply)
        return QDialogButtonBox::Yes;
    if (result == QDialogButtonBox::Discard)
        return QDialogButtonBox::No;

    return QDialogButtonBox::Cancel;
}

void QnServerSettingsDialog::updateWebPageLink()
{
    bool allowed = m_server && !nx::network::isCloudServer(m_server);

    m_webPageLink->setVisible(allowed);
    m_webPageLink->setEnabled(allowed && m_server->getStatus() == Qn::Online);
}
