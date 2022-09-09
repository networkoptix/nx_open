// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_widget.h"
#include "ui_backup_settings_widget.h"

#include <algorithm>

#include <QtWidgets/QTabBar>
#include <QtCore/QTimer>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/system_settings.h>
#include <server/server_storage_manager.h>
#include <ui/common/palette.h>

namespace {

static constexpr auto kPlaceholderImageSize = QSize(128, 128);
static constexpr int kPlaceholderTextPixelSize = 16;
static constexpr auto kPlaceholderTextWeight = QFont::Light;

bool backupStorageConfigured(const QnMediaServerResourcePtr& server)
{
    if (server.isNull())
        return false;

    const auto storageResourceList = server->getStorages();
    return std::any_of(std::cbegin(storageResourceList), std::cend(storageResourceList),
        [](const auto& storageResource)
        {
            const auto statusFlags = storageResource->statusFlag();
            return !statusFlags.testFlag(nx::vms::api::StorageStatus::disabled)
                && storageResource->isBackup()
                && storageResource->isUsedForWriting();
        });
}

int enabledStoragesCount(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return 0;

    const auto storageResourceList = server->getStorages();
    return std::count_if(std::cbegin(storageResourceList), std::cend(storageResourceList),
        [](const auto& storageResource)
        {
            const auto statusFlags = storageResource->statusFlag();
            return !statusFlags.testFlag(nx::vms::api::StorageStatus::disabled)
                && storageResource->isUsedForWriting();
        });
}

} // namespace

namespace nx::vms::client::desktop {

BackupSettingsWidget::BackupSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::BackupSettingsWidget())
{
    ui->setupUi(this);

    setupPlaceholderPageAppearance();

    setWarningStyle(ui->offlinePlaceholderPage);

    connect(ui->unconfiguredPlaceholderLabel, &QLabel::linkActivated,
        this, &BackupSettingsWidget::storageManagementShortcutClicked);

    using namespace nx::style;
    ui->emptyTabBarFiller->setProperty(Properties::kTabShape, static_cast<int>(TabShape::Compact));
    ui->tabBar->setProperty(Properties::kTabShape, static_cast<int>(TabShape::Compact));

    ui->tabBar->addTab(tr("Settings"));
    ui->tabBar->addTab(tr("Bandwidth Limit"));
    connect(ui->tabBar, &QTabBar::currentChanged,
        ui->settingsStackedWidget, &QStackedWidget::setCurrentIndex);

    // Widget initialized with this pointer as parent to setup workbench context.
    m_backupSettingsViewWidget = new BackupSettingsViewWidget(this);

    // Ownership of widget is transferred to the layout.
    ui->viewWidgetPageLayout->insertWidget(0, m_backupSettingsViewWidget);

    connect(qnServerStorageManager, &QnServerStorageManager::storageAdded, this,
        [this] { updateBackupSettingsAvailability(); });

    connect(qnServerStorageManager, &QnServerStorageManager::storageChanged, this,
        [this] { updateBackupSettingsAvailability(); });

    connect(qnServerStorageManager, &QnServerStorageManager::storageRemoved, this,
        [this] { updateBackupSettingsAvailability(); });

    connect(m_backupSettingsViewWidget, &BackupSettingsViewWidget::hasChangesChanged,
        this, &BackupSettingsWidget::hasChangesChanged);

    connect(m_backupSettingsViewWidget, &BackupSettingsViewWidget::globalBackupSettingsChanged,
        this, &BackupSettingsWidget::updateMessageBarText);

    connect(ui->backupBandwidthSettingsWidget, &BackupBandwidthSettingsWidget::hasChangesChanged,
        this, &BackupSettingsWidget::hasChangesChanged);
}

BackupSettingsWidget::~BackupSettingsWidget()
{
}

void BackupSettingsWidget::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    if (m_server)
        m_server->disconnect(this);

    m_server = server;
    ui->backupStatusWidget->setServer(m_server);
    ui->backupBandwidthSettingsWidget->setServer(m_server);

    if (m_server)
    {
        connect(m_server.get(),
            &QnResource::statusChanged,
            this,
            &BackupSettingsWidget::updateBackupSettingsAvailability);
    }
}

bool BackupSettingsWidget::hasChanges() const
{
    return m_backupSettingsViewWidget->hasChanges()
        || ui->backupBandwidthSettingsWidget->hasChanges();
}

void BackupSettingsWidget::loadDataToUi()
{
    if (m_server.isNull())
        return;

    m_backupSettingsViewWidget->setTreeEntityFactoryFunction(
        [this](const entity_resource_tree::ResourceTreeEntityBuilder* builder)
        {
            using namespace entity_item_model;

            AbstractItemPtr newAddedCamerasItem = GenericItemBuilder()
                .withRole(Qn::ResourceIconKeyRole, static_cast<int>(QnResourceIconCache::Cameras))
                .withRole(Qt::DisplayRole, tr("New added cameras"))
                .withRole(ResourceDialogItemRole::NewAddedCamerasItemRole, true)
                .withFlags({Qt::ItemIsEnabled, Qt::ItemNeverHasChildren});

            return builder->addPinnedItem(builder->createDialogServerCamerasEntity(m_server, {}),
                std::move(newAddedCamerasItem));
        });
    m_backupSettingsViewWidget->loadDataToUi();
    m_backupSettingsViewWidget->resourceViewWidget()->makeRequiredItemsVisible();
    ui->backupBandwidthSettingsWidget->loadDataToUi();

    updateMessageBarText();
    updateBackupSettingsAvailability();
}

void BackupSettingsWidget::applyChanges()
{
    using namespace std::chrono;
    static constexpr auto kStatusUpdateTimeout = 500ms;

    m_backupSettingsViewWidget->applyChanges();
    ui->backupBandwidthSettingsWidget->applyChanges();
    QTimer::singleShot(kStatusUpdateTimeout,
        ui->backupStatusWidget, &BackupStatusWidget::updateBackupStatus);
    updateMessageBarText();
}

void BackupSettingsWidget::discardChanges()
{
    m_backupSettingsViewWidget->discardChanges();
    ui->backupBandwidthSettingsWidget->discardChanges();
}

void BackupSettingsWidget::setupPlaceholderPageAppearance()
{
    QFont placeholderFont;
    placeholderFont.setPixelSize(kPlaceholderTextPixelSize);
    placeholderFont.setWeight(kPlaceholderTextWeight);

    setPaletteColor(ui->unconfiguredPlaceholderLabel, QPalette::WindowText,
        colorTheme()->color("dark17"));
    ui->unconfiguredPlaceholderLabel->setFont(placeholderFont);

    const auto pixmap = qnSkin->pixmap(
        "placeholders/backup_placeholder.svg", true, kPlaceholderImageSize);
    ui->unconfiguredPlaceholderImageLabel->setPixmap(qnSkin->maximumSizePixmap(pixmap));
}

void BackupSettingsWidget::updateBackupSettingsAvailability()
{
    const bool serverIsOffline = m_server.isNull() || !m_server->isOnline();
    const bool storageConfigured = backupStorageConfigured(m_server);
    if (!storageConfigured && hasChanges())
        discardChanges();

    if (serverIsOffline)
    {
        ui->stackedWidget->setCurrentWidget(ui->offlinePlaceholderPage);

        return;
    }

    if (!storageConfigured)
    {
        const auto storageManagementLink =
            QString("<a href=\"#\" style=\"text-decoration:none\">%1</a>")
               .arg(tr("Storage Management"));
        auto storagesCount = enabledStoragesCount(m_server);
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
        ui->stackedWidget->setCurrentWidget(ui->unconfiguredPlaceholderPage);
    }
    else
    {
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    }
}

void BackupSettingsWidget::updateMessageBarText()
{
    const auto savedBackupSettings = systemSettings()->backupSettings();
    const auto modelBackupSettings = m_backupSettingsViewWidget->globalBackupSettings();

    if (savedBackupSettings == modelBackupSettings)
    {
        ui->messageBar->setText({});
    }
    else if (savedBackupSettings.quality != modelBackupSettings.quality)
    {
        ui->messageBar->setText(
            tr("New added cameras settings will apply to all servers in the system."));
    }
    else
    {
        ui->messageBar->setText(modelBackupSettings.backupNewCameras
            ? tr("Backup will be turned on for new added cameras on all servers in the system.")
            : tr("Backup will be turned off for new added cameras on all servers in the system."));
    }
}

} // namespace nx::vms::client::desktop
