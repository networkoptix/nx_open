// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <QtCore/QPointer>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/network/cloud_url_validator.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_properties/server/poe/poe_settings_widget.h>
#include <nx/vms/client/desktop/resource_properties/server/watchers/server_plugin_data_watcher.h>
#include <nx/vms/client/desktop/resource_properties/server/watchers/server_settings_backup_storages_watcher.h>
#include <nx/vms/client/desktop/resource_properties/server/watchers/server_settings_saas_state_watcher.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/backup_settings_widget.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/server_plugins_settings_widget.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <ui/widgets/properties/server_settings_widget.h>
#include <ui/widgets/properties/storage_analytics_widget.h>
#include <ui/widgets/properties/storage_config_widget.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>

using namespace nx::vms::client::desktop;

struct QnServerSettingsDialog::Private
{
    QnServerSettingsDialog* const q;
    QnMediaServerResourcePtr server;
    const QPointer<ServerSettingsDialogStore> store;
    const QPointer<ServerPluginDataWatcher> pluginDataWatcher;
    const QPointer<ServerSettingsBackupStoragesWatcher> backupStoragesWatcher;
    const QPointer<ServerSettingsSaasStateWatcher> saasStateWatcher;

    QnServerSettingsWidget* const generalPage;
    QnStorageAnalyticsWidget* const statisticsPage;
    QnStorageConfigWidget* const storagesPage;
    PoeSettingsWidget* const poeSettingsPage;
    ServerPluginsSettingsWidget* const pluginsPage;
    BackupSettingsWidget* const backupPage;
    QLabel* const webPageLink;

    Private(QnServerSettingsDialog* q):
        q(q),
        store(new ServerSettingsDialogStore(q)),
        pluginDataWatcher(new ServerPluginDataWatcher(store, q)),
        backupStoragesWatcher(new ServerSettingsBackupStoragesWatcher(store, q)),
        saasStateWatcher(new ServerSettingsSaasStateWatcher(store, q)),
        generalPage(new QnServerSettingsWidget(q)),
        statisticsPage(new QnStorageAnalyticsWidget(q)),
        storagesPage(new QnStorageConfigWidget(q)),
        poeSettingsPage(new PoeSettingsWidget(q)),
        pluginsPage(ini().pluginInformationInServerSettings
            ? new ServerPluginsSettingsWidget(store, appContext()->qmlEngine(), q)
            : nullptr),
        backupPage(new BackupSettingsWidget(store, q)),
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
    };

    void updatePoeSettingsPageVisibility()
    {
        q->setPageVisible(
            PoePage,
            server && server->getServerFlags().testFlag(nx::vms::api::SF_HasPoeManagementCapability));
    }
};

//--------------------------------------------------------------------------------------------------

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
    addPage(PoePage, d->poeSettingsPage, tr("PoE"));
    addPage(BackupPage, d->backupPage, tr("Backup"));
    d->updatePoeSettingsPageVisibility();

    if (ini().pluginInformationInServerSettings)
        addPage(PluginsPage, new Private::PluginSettingsAdapter(d.get()), tr("Plugins"));

    connect(d->backupPage, &BackupSettingsWidget::storageManagementShortcutClicked, this,
        [this]() { ui->tabWidget->setCurrentWidget(d->storagesPage); });

    setupShowWebServerLink();

    connect(system()->resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            if (resources.contains(d->server))
            {
                tryClose(/*force*/ true);
                setServer({});
            }
        });

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this,
        [this](const QnResourceList& resources)
        {
            if (isHidden())
                return;

            const auto servers = resources.filtered<QnMediaServerResource>(
                [](const QnMediaServerResourcePtr &server)
                {
                    return server.dynamicCast<QnMediaServerResource>();
                });

            if (!servers.isEmpty())
                setServer(servers.first());
        });

    setHelpTopic(this, HelpTopic::Id::ServerSettings_General);
}

QnServerSettingsDialog::~QnServerSettingsDialog()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void QnServerSettingsDialog::setupShowWebServerLink()
{
    // The button box is initialized in the base class on polish event.
    ensurePolished();

    const auto buttonsLayout = qobject_cast<QHBoxLayout*>(buttonBox()->layout());
    if (!buttonsLayout)
    {
        NX_ASSERT(false, "Invalid QDialogButtonBox layout");
        return;
    }

    buttonsLayout->insertWidget(0, d->webPageLink);
    setHelpTopic(d->webPageLink, HelpTopic::Id::ServerSettings_WebClient);
    connect(d->webPageLink, &QLabel::linkActivated, this,
        [this] { menu()->trigger(menu::WebAdminAction, d->server); });
}

QnMediaServerResourcePtr QnServerSettingsDialog::server() const
{
    return d->server;
}

void QnServerSettingsDialog::setServer(const QnMediaServerResourcePtr& server)
{
    if (d->server == server)
        return;

    if (!isHidden() && server && hasChanges() && !switchServerWithConfirmation())
        return;

    if (d->server)
        d->server->disconnect(this);

    d->server = server;

    if (d->store)
        d->store->loadServer(d->server);

    if (d->pluginDataWatcher)
        d->pluginDataWatcher->setServer(server);

    if (d->backupStoragesWatcher)
        d->backupStoragesWatcher->setServer(server);

    if (d->saasStateWatcher)
        d->saasStateWatcher->setServer(server);

    d->generalPage->setServer(server);
    d->statisticsPage->setServer(server);
    d->storagesPage->setServer(server);
    d->poeSettingsPage->setServerId(server ? server->getId() : QnUuid());
    d->backupPage->setServer(server);

    if (d->server)
    {
        connect(d->server.get(), &QnResource::statusChanged,
            this, &QnServerSettingsDialog::updateWebPageLink);
    }

    loadDataToUi();
    updateWebPageLink();
    d->updatePoeSettingsPageVisibility();
}

void QnServerSettingsDialog::retranslateUi()
{
    base_type::retranslateUi();
    if (d->server)
    {
        const bool readOnly = !system()->accessController()->hasPermissions(d->server,
            Qn::WritePermission | Qn::SavePermission);

        setWindowTitle(readOnly
            ? tr("Server Settings - %1 (readonly)").arg(d->server->getName())
            : tr("Server Settings - %1").arg(d->server->getName()));
    }
    else
    {
        setWindowTitle(tr("Server Settings"));
    }
}

void QnServerSettingsDialog::discardChanges()
{
    base_type::discardChanges();
    NX_ASSERT(!isNetworkRequestRunning());
}

bool QnServerSettingsDialog::switchServerWithConfirmation()
{
    if (!NX_ASSERT(d->server, "Server must exist here"))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Icon::Question,
        tr("Apply changes before switching to another server?"),
        /*extraMessage*/ QString(),
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply,
        this);

    if (!canApplyChanges())
        messageBox.button(QDialogButtonBox::Apply)->setEnabled(false);

    const auto result = static_cast<QDialogButtonBox::StandardButton>(messageBox.exec());

    // Logic of requests canceling or waiting will not work if we switch to another server before
    // closing the dialog. That's why we should wait for network request completion here.
    if (result == QDialogButtonBox::Apply)
        applyChangesSync();
    else if (result == QDialogButtonBox::Discard)
        discardChangesSync();
    else // result == QDialogButtonBox::Cancel
        return false;

    return true;
}

bool QnServerSettingsDialog::event(QEvent* e)
{
    const auto type = e->type();
    if (type == QEvent::Hide || type == QEvent::Show)
        d->poeSettingsPage->setAutoUpdate(type == QEvent::Show);

    return base_type::event(e);
}

void QnServerSettingsDialog::updateWebPageLink()
{
    bool allowed = d->server && !isCloudServer(d->server);

    d->webPageLink->setVisible(allowed);
    d->webPageLink->setEnabled(allowed
        && d->server->getStatus() == nx::vms::api::ResourceStatus::online);
    if (allowed)
    {
        d->webPageLink->setText(nx::vms::common::html::link(
            tr("Server Web Page"),
            nx::utils::Url(d->server->getUrl())));
    }
}
