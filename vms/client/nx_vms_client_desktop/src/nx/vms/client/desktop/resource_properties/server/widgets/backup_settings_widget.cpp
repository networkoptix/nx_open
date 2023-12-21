// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_widget.h"
#include "ui_backup_settings_widget.h"

#include <algorithm>

#include <QtCore/QTimer>
#include <QtWidgets/QTabBar>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>
#include <server/server_storage_manager.h>
#include <ui/common/palette.h>

namespace {

static constexpr auto kPlaceholderImageSize = QSize(128, 128);
static constexpr int kPlaceholderTextPixelSize = 16;
static constexpr auto kPlaceholderTextWeight = QFont::Light;

QFont placeholderMessageCaptionFont()
{
    static constexpr auto kPlaceholderMessageCaptionFontSize = 16;
    static constexpr auto kPlaceholderMessageCaptionFontWeight = QFont::Medium;

    QFont font;
    font.setPixelSize(kPlaceholderMessageCaptionFontSize);
    font.setWeight(kPlaceholderMessageCaptionFontWeight);
    return font;
}

QFont placeholderMessageFont()
{
    static constexpr auto kPlaceholderMessageFontSize = 14;
    static constexpr auto kPlaceholderMessageFontWeight = QFont::Normal;

    QFont font;
    font.setPixelSize(kPlaceholderMessageFontSize);
    font.setWeight(kPlaceholderMessageFontWeight);
    return font;
}

} // namespace

namespace nx::vms::client::desktop {

BackupSettingsWidget::BackupSettingsWidget(ServerSettingsDialogStore* store, QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::BackupSettingsWidget())
{
    ui->setupUi(this);

    setupPlaceholders();

    using namespace nx::style;
    ui->emptyTabBarFiller->setProperty(Properties::kTabShape, static_cast<int>(TabShape::Compact));
    ui->tabBar->setProperty(Properties::kTabShape, static_cast<int>(TabShape::Compact));

    ui->tabBar->addTab(tr("Settings"));
    ui->tabBar->addTab(tr("Bandwidth Limit"));
    connect(ui->tabBar, &QTabBar::currentChanged,
        ui->settingsStackedWidget, &QStackedWidget::setCurrentIndex);

    // Widget initialized with this pointer as parent to setup workbench context.
    m_backupSettingsViewWidget = new BackupSettingsViewWidget(store, this);

    // Ownership of widget is transferred to the layout.
    ui->viewWidgetPageLayout->insertWidget(0, m_backupSettingsViewWidget);

    connect(store, &ServerSettingsDialogStore::stateChanged, this,
        &BackupSettingsWidget::loadState);

    connect(m_backupSettingsViewWidget, &BackupSettingsViewWidget::hasChangesChanged,
        this, &BackupSettingsWidget::hasChangesChanged);

    connect(ui->backupBandwidthSettingsWidget, &BackupBandwidthSettingsWidget::hasChangesChanged,
        this, &BackupSettingsWidget::hasChangesChanged);
}

BackupSettingsWidget::~BackupSettingsWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void BackupSettingsWidget::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_server = server;
    ui->backupStatusWidget->setServer(m_server);
    ui->backupBandwidthSettingsWidget->setServer(m_server);
}

bool BackupSettingsWidget::hasChanges() const
{
    return m_backupSettingsViewWidget->hasChanges()
        || ui->backupBandwidthSettingsWidget->hasChanges();
}

void BackupSettingsWidget::applyChanges()
{
    using namespace std::chrono;
    static constexpr auto kStatusUpdateTimeout = 500ms;

    m_backupSettingsViewWidget->applyChanges();
    ui->backupBandwidthSettingsWidget->applyChanges();
    QTimer::singleShot(kStatusUpdateTimeout,
        ui->backupStatusWidget, &BackupStatusWidget::updateBackupStatus);
}

void BackupSettingsWidget::discardChanges()
{
    m_backupSettingsViewWidget->discardChanges();
    ui->backupBandwidthSettingsWidget->discardChanges();
    NX_ASSERT(!isNetworkRequestRunning());
}

bool BackupSettingsWidget::isNetworkRequestRunning() const
{
    return m_backupSettingsViewWidget->isNetworkRequestRunning();
}

void BackupSettingsWidget::loadState(const ServerSettingsDialogState& state)
{
    if (!state.isOnline)
    {
        ui->stackedWidget->setCurrentWidget(ui->genericPlaceholderPage);
        ui->genericPlaceholderCaptionLabel->setText(tr("Server is offline"));
        ui->genericPlaceholderMessageLabel->setText(tr("Backup settings are not available"));
        m_backupSettingsViewWidget->setTreeEntityFactoryFunction({});
        return;
    }

    if (!state.backupStoragesStatus.hasActiveBackupStorage)
    {
        const auto storageManagementLink =
            QString("<a href=\"#\" style=\"text-decoration:none\">%1</a>")
               .arg(tr("Storage Management"));
        auto storagesCount = state.backupStoragesStatus.enabledNonCloudStoragesCount;
        QString labelStringTemplate;
        if (storagesCount > 1)
        {
            //: "Storage Management" link will be substituted as %1.
            labelStringTemplate = tr("To enable backup change \"Main\" to \"Backup\" "
                "from some of the storages in %1");
        }
        else
        {
            //: "Storage Management" link will be substituted as %1.
            labelStringTemplate =
                tr("To enable backup add more drives to use them as backup storage in %1");
        }
        ui->unconfiguredPlaceholderLabel->setText(labelStringTemplate.arg(storageManagementLink));
        ui->stackedWidget->setCurrentWidget(ui->noActiveBackupStoragePlaceholder);
        m_backupSettingsViewWidget->setTreeEntityFactoryFunction({});
    }
    else
    {
        using namespace nx::vms::common::saas;

        const auto cloudBackupStorage = state.backupStoragesStatus.usesCloudBackupStorage;
        const auto saasState = state.saasProperties.saasState;

        if (ServiceManager::saasShutDown(saasState) && cloudBackupStorage)
        {
            ui->stackedWidget->setCurrentWidget(ui->genericPlaceholderPage);
            ui->genericPlaceholderCaptionLabel->setText(tr("System shut down"));
            ui->genericPlaceholderMessageLabel->setText(
                tr("To perform backup to a cloud storage, the System should be in active state. %1")
                    .arg(StringsHelper::recommendedAction(saasState)).trimmed());

            m_backupSettingsViewWidget->setTreeEntityFactoryFunction({});
            return;
        }

        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
        m_backupSettingsViewWidget->setTreeEntityFactoryFunction(
            [this, cloudBackupStorage = state.backupStoragesStatus.usesCloudBackupStorage]
            (const entity_resource_tree::ResourceTreeEntityBuilder* builder)
            {
                using namespace entity_item_model;

                auto serverCamerasEntity =
                    builder->createDialogServerCamerasEntity(m_server, {});

                if (cloudBackupStorage)
                    return serverCamerasEntity;

                AbstractItemPtr newAddedCamerasItem = GenericItemBuilder()
                    .withRole(Qn::ResourceIconKeyRole, static_cast<int>(QnResourceIconCache::Cameras))
                    .withRole(Qt::DisplayRole, tr("New added cameras"))
                    .withRole(Qn::ExtraInfoRole, tr("\u2013 Applies to all servers")) //< EnDash
                    .withRole(Qn::ForceExtraInfoRole, true)
                    .withRole(ResourceDialogItemRole::NewAddedCamerasItemRole, true)
                    .withFlags({Qt::ItemIsEnabled, Qt::ItemNeverHasChildren});

                return builder->addPinnedItem(
                    std::move(serverCamerasEntity),
                    std::move(newAddedCamerasItem));
            });

        QString alertBarMessage;
        if (cloudBackupStorage && saasState == nx::vms::api::SaasState::suspend)
        {
            alertBarMessage = tr("System suspended. To make changes to backup configuration, the "
                "System should be in active state. %1")
                    .arg(nx::vms::common::saas::StringsHelper::recommendedAction(saasState));
            m_backupSettingsViewWidget->resourceViewWidget()->footerWidget()->setHidden(true);
        }
        else
        {
            m_backupSettingsViewWidget->resourceViewWidget()->footerWidget()->setHidden(false);
        }
        m_backupSettingsViewWidget->resourceViewWidget()->setInvalidMessage(alertBarMessage);

        m_backupSettingsViewWidget->resourceViewWidget()->makeRequiredItemsVisible();
        ui->backupBandwidthSettingsWidget->loadDataToUi();
    }

    if (!state.backupStoragesStatus.hasActiveBackupStorage && hasChanges())
        discardChanges();
}

void BackupSettingsWidget::setupPlaceholders()
{
    // No active backup storage placeholder.
    QFont placeholderFont;
    placeholderFont.setPixelSize(kPlaceholderTextPixelSize);
    placeholderFont.setWeight(kPlaceholderTextWeight);

    setPaletteColor(ui->unconfiguredPlaceholderLabel, QPalette::WindowText,
        core::colorTheme()->color("dark17"));
    ui->unconfiguredPlaceholderLabel->setFont(placeholderFont);

    const auto pixmap = qnSkin->pixmap(
        "placeholders/backup_placeholder.svg", true, kPlaceholderImageSize);
    ui->unconfiguredPlaceholderImageLabel->setPixmap(qnSkin->maximumSizePixmap(pixmap));

    connect(ui->unconfiguredPlaceholderLabel, &QLabel::linkActivated,
        this, &BackupSettingsWidget::storageManagementShortcutClicked);

    // 'Server offline' / 'SaaS suspended' / 'SaaS shut down' placeholder.
    ui->genericPlaceholderCaptionLabel->setFont(placeholderMessageCaptionFont());
    ui->genericPlaceholderMessageLabel->setFont(placeholderMessageFont());
}

} // namespace nx::vms::client::desktop
