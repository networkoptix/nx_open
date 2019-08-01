#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <QtCore/QPointer>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <client_core/client_core_module.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <network/cloud_url_validator.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/properties/server_settings_widget.h>
#include <ui/widgets/properties/storage_analytics_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/widgets/properties/storage_config_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <utils/common/html.h>

#include <nx/network/http/http_types.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/resource_properties/server/redux/server_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/server/redux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_properties/server/watchers/server_plugin_data_watcher.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/server_plugins_settings_widget.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

struct QnServerSettingsDialog::Private
{
    QnMediaServerResourcePtr server;
    const QPointer<ServerSettingsDialogStore> store;
    const QPointer<ServerPluginDataWatcher> pluginDataWatcher;

    QnServerSettingsWidget* const generalPage;
    QnStorageAnalyticsWidget* const statisticsPage;
    QnStorageConfigWidget* const storagesPage;
    ServerPluginsSettingsWidget* const pluginsPage;
    QLabel* const webPageLink;

    Private(QnServerSettingsDialog* q):
        store(new ServerSettingsDialogStore(q)),
        pluginDataWatcher(new ServerPluginDataWatcher(store, q)),
        generalPage(new QnServerSettingsWidget(q)),
        statisticsPage(new QnStorageAnalyticsWidget(q)),
        storagesPage(new QnStorageConfigWidget(q)),
        pluginsPage(ini().pluginInformationInServerSettings
            ? new ServerPluginsSettingsWidget(store, qnClientCoreModule->mainQmlEngine(), q)
            : nullptr),
        webPageLink(new QLabel(q))
    {
    }

    // This is an adapter for FLUX-based ServerPluginsSettingsWidget to be used in this
    // non-FLUX dialog.
    class PluginSettingsAdapter: public QnAbstractPreferencesWidget
    {
        Private* const d;
        using base_type = QnAbstractPreferencesWidget;

    public:
        PluginSettingsAdapter(Private* d): d(d)
        {
            d->pluginsPage->setParent(this);
            anchorWidgetToParent(d->pluginsPage);
        }

        virtual bool hasChanges() const override { return false; }
        virtual void applyChanges() override {}
        virtual void loadDataToUi() override {}
    };
};

QnServerSettingsDialog::QnServerSettingsDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    d(new Private(this))
{
    ui->setupUi(this);
    setTabWidget(ui->tabWidget);

    addPage(SettingsPage, d->generalPage, tr("General"));
    addPage(StorageManagmentPage, d->storagesPage, tr("Storage Management"));
    addPage(StatisticsPage, d->statisticsPage, tr("Storage Analytics"));

    if (ini().pluginInformationInServerSettings)
        addPage(PluginsPage, new Private::PluginSettingsAdapter(d.get()), tr("Plugins"));

    setupShowWebServerLink();

    // TODO: #GDM #access connect to resource pool to check if server was deleted
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

    d->webPageLink->setText(makeHref(tr("Server Web Page"), lit("#")));
    buttonsLayout->insertWidget(0, d->webPageLink);
    setHelpTopic(d->webPageLink, Qn::ServerSettings_WebClient_Help);
    connect(d->webPageLink, &QLabel::linkActivated, this,
        [this] { menu()->trigger(action::WebAdminAction, d->server); });
}

QnMediaServerResourcePtr QnServerSettingsDialog::server() const
{
    return d->server;
}

void QnServerSettingsDialog::setServer(const QnMediaServerResourcePtr& server)
{
    if (d->server == server)
        return;

    if (!tryClose(false))
        return;

    if (d->server)
        d->server->disconnect(this);

    d->server = server;

    if (d->store)
        d->store->loadServer(d->server);

    if (d->pluginDataWatcher)
        d->pluginDataWatcher->setServer(server);

    d->generalPage->setServer(server);
    d->statisticsPage->setServer(server);
    d->storagesPage->setServer(server);

    if (d->server)
    {
        connect(d->server, &QnResource::statusChanged,
            this, &QnServerSettingsDialog::updateWebPageLink);
    }

    loadDataToUi();
    updateWebPageLink();
}

void QnServerSettingsDialog::retranslateUi()
{
    base_type::retranslateUi();
    if (d->server)
    {
        bool readOnly = !accessController()->hasPermissions(d->server, Qn::WritePermission | Qn::SavePermission);
        setWindowTitle(readOnly
            ? tr("Server Settings - %1 (readonly)").arg(d->server->getName())
            : tr("Server Settings - %1").arg(d->server->getName()));
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
    NX_ASSERT(d->server, "Server must exist here");

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
    bool allowed = d->server && !nx::network::isCloudServer(d->server);

    d->webPageLink->setVisible(allowed);
    d->webPageLink->setEnabled(allowed && d->server->getStatus() == Qn::Online);
}
