#include "multi_server_updates_widget.h"

#include "ui_multi_server_updates_widget.h"

#include <functional>

#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

#include <api/global_settings.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>
#include <client/client_app_info.h>

#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench_context.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/models/resource/resource_list_model.h>

#include <network/system_helpers.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/eula_dialog.h>
#include <nx/vms/client/desktop/workbench/extensions/workbench_progress_manager.h>
#include <nx/network/app_info.h>
#include <nx/utils/app_info.h>

#include <nx/vms/client/desktop/ini.h>
#include <utils/common/html.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/applauncher_utils.h>

#include "workbench_update_watcher.h"
#include "peer_state_tracker.h"
#include "server_update_tool.h"
#include "server_updates_model.h"
#include "server_status_delegate.h"

using namespace nx::vms::client::desktop::ui;

namespace {

const auto kLongInstallWarningTimeout = std::chrono::minutes(2);
const int kTooLateDayOfWeek = Qt::Thursday;
const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour
const int kVersionLabelFontSizePixels = 24;
const int kVersionLabelFontWeight = QFont::DemiBold;

// Height limit for servers list in dialog box with update report
static constexpr int kSectionHeight = 150;
static constexpr int kSectionWidth = 330;
static constexpr int kSectionMinHeight = 40;

constexpr auto kLatestVersionBannerLabelFontSizePixels = 22;
constexpr auto kLatestVersionBannerLabelFontWeight = QFont::Light;

const auto kWaitForUpdateCheckFuture = std::chrono::milliseconds(1);
const auto kDelayForCheckingInstallStatus = std::chrono::minutes(1);
const auto kPeriodForCheckingInstallStatus = std::chrono::seconds(10);

const int kLinkCopiedMessageTimeoutMs = 2000;

// Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
const quint32 kProcessTerminateTimeoutMs = 15000;

/* N-dash 5 times: */
const QString kNoVersionNumberText = QString::fromWCharArray(L"\x2013\x2013\x2013\x2013\x2013");

constexpr int kVersionSelectionBlockHeight = 20;
constexpr int kVersionInformationBlockMinHeight = 48;
constexpr int kPreloaderHeight = 32;

// Adds resource list to message box
QTableView* injectResourceList(
    QnMessageBox& messageBox, const QnResourceList& resources)
{
    if (resources.empty())
        return nullptr;

    auto resourceList = new nx::vms::client::desktop::TableView();
    resourceList->setShowGrid(false);

    auto resourceListModel = new QnResourceListModel(resourceList);
    resourceListModel->setHasStatus(false);
    resourceListModel->setResources(resources);
    resourceList->setModel(resourceListModel);

    auto horisontalHeader = resourceList->horizontalHeader();
    horisontalHeader->hide();
    horisontalHeader->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);

    auto verticalHeader = resourceList->verticalHeader();
    verticalHeader->hide();
    resourceList->setMinimumHeight(kSectionMinHeight);
    resourceList->setMaximumHeight(kSectionHeight);
    resourceList->setMaximumWidth(kSectionWidth);
    resourceList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    resourceList->resizeRowsToContents();

    // Adding this to QnMessageBox::Layout::Main looks ugly.
    // Adding to Layout::BeforeAdditionalInfo (default value) looks much better.
    messageBox.addCustomWidget(resourceList, QnMessageBox::Layout::BeforeAdditionalInfo);
    return resourceList;
}

} // anonymous namespace

namespace nx::vms::client::desktop
{

MultiServerUpdatesWidget::MultiServerUpdatesWidget(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    ui(new Ui::MultiServerUpdatesWidget)
{
    ui->setupUi(this);

    ui->titleStackedWidget->setFixedHeight(kVersionSelectionBlockHeight);
    ui->versionStackedWidget->setMinimumHeight(kVersionInformationBlockMinHeight);
    ui->updateStackedWidget->setFixedHeight(style::Metrics::kButtonHeight);

    ui->checkingProgress->setFixedHeight(kPreloaderHeight);

    autoResizePagesToContents(ui->versionStackedWidget,
        QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));

    m_showDebugData = ini().massSystemUpdateDebugInfo;
    m_autoCheckUpdate = qnGlobalSettings->isUpdateNotificationsEnabled();

    auto watcher = context()->instance<nx::vms::client::desktop::WorkbenchUpdateWatcher>();
    m_serverUpdateTool = watcher->getServerUpdateTool();
    NX_ASSERT(m_serverUpdateTool);

    m_clientUpdateTool.reset(new ClientUpdateTool(this));

    m_updateCheck = watcher->takeUpdateCheck();

    m_updatesModel = m_serverUpdateTool->getModel();
    m_stateTracker = m_serverUpdateTool->getStateTracker();
    m_stateTracker->setServerFilter(
        [](const QnMediaServerResourcePtr& server) -> bool
        {
            return helpers::serverBelongsToCurrentSystem(server);
        });
    m_stateTracker->setResourceFeed(resourcePool());

    QFont versionLabelFont;
    versionLabelFont.setPixelSize(kVersionLabelFontSizePixels);
    versionLabelFont.setWeight(kVersionLabelFontWeight);
    ui->targetVersionLabel->setFont(versionLabelFont);
    ui->targetVersionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->targetVersionLabel->setForegroundRole(QPalette::Text);

    QFont latestVersionBannerFont;
    latestVersionBannerFont.setPixelSize(kLatestVersionBannerLabelFontSizePixels);
    latestVersionBannerFont.setWeight(kLatestVersionBannerLabelFontWeight);
    ui->latestVersionBannerLabel->setFont(latestVersionBannerFont);
    ui->latestVersionBannerLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->latestVersionBannerLabel->setForegroundRole(QPalette::Text);
    ui->latestVersionIconLabel->setPixmap(qnSkin->pixmap("system_settings/update_checkmark.png"));
    ui->linkCopiedIconLabel->setPixmap(qnSkin->pixmap("text_buttons/ok.png"));
    ui->linkCopiedWidget->hide();

    setHelpTopic(this, Qn::Administration_Update_Help);

    m_sortedModel.reset(new SortedPeerUpdatesModel(this));
    m_sortedModel->setSourceModel(m_updatesModel.get());
    ui->tableView->setModel(m_sortedModel.get());
    m_statusItemDelegate.reset(new ServerStatusItemDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(ServerUpdatesModel::ProgressColumn,
        m_statusItemDelegate.get());

    m_resourceNameDelegate.reset(new QnResourceItemDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(ServerUpdatesModel::NameColumn,
        m_resourceNameDelegate.get());

    connect(m_sortedModel.get(), &SortedPeerUpdatesModel::dataChanged,
        this, &MultiServerUpdatesWidget::atModelDataChanged);

    // the column does not matter because the model uses column-independent sorting
    m_sortedModel->sort(0);

    if (auto horizontalHeader = ui->tableView->horizontalHeader())
    {
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::NameColumn,
            QHeaderView::ResizeToContents);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::VersionColumn,
            QHeaderView::ResizeToContents);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::ProgressColumn,
            QHeaderView::Stretch);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::StatusMessageColumn,
            QHeaderView::Stretch);
        horizontalHeader->setSectionsClickable(false);
        horizontalHeader->setStretchLastSection(true);
    }
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::ProgressColumn, false);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::packageDownloaded,
        this, &MultiServerUpdatesWidget::atServerPackageDownloaded);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::packageDownloadFailed,
        this, &MultiServerUpdatesWidget::atServerPackageDownloadFailed);

    connect(m_clientUpdateTool.get(), &ClientUpdateTool::updateStateChanged,
        m_stateTracker.get(), &PeerStateTracker::atClientUpdateStateChanged);

    connect(ui->cancelProgressAction, &QPushButton::clicked, this,
        &MultiServerUpdatesWidget::atCancelCurrentAction);

    connect(ui->cancelUpdateButton, &QPushButton::clicked, this,
        &MultiServerUpdatesWidget::atCancelCurrentAction);

    connect(ui->downloadButton, &QPushButton::clicked,
        this, &MultiServerUpdatesWidget::atStartUpdateAction);

    connect(ui->browseUpdate, &QPushButton::clicked,
        this, [this]()
        {
            if (m_updateSourceMode == nx::update::UpdateSourceType::internet)
            {
                // We should be here only if we have latest version and want to check specific build
                setUpdateSourceMode(UpdateSourceType::internetSpecific);
            }
            else if (m_updateSourceMode == UpdateSourceType::file)
            {
                pickLocalFile();
            }
            else if (m_updateSourceMode == UpdateSourceType::internetSpecific)
            {
                pickSpecificBuild();
            }
        });

    connect(ui->advancedUpdateSettings, &QPushButton::clicked,
        this, [this]()
        {
            auto settingsVisible = !m_showStorageSettings;

            m_showStorageSettings = settingsVisible;
            m_updateRemoteStateChanged = true;

            loadDataToUi();
        });

    ui->advancedUpdateSettings->setIcon(qnSkin->icon("text_buttons/collapse.png"));

    setAccentStyle(ui->downloadButton);
    // This button is hidden for now. We will implement it in future.
    ui->downloadAndInstall->hide();

    connect(ui->releaseNotesUrl, &QLabel::linkActivated, this,
        [this]()
        {
            if (m_updateInfo.isValidToInstall() && !m_updateInfo.info.releaseNotesUrl.isEmpty())
                QDesktopServices::openUrl(m_updateInfo.info.releaseNotesUrl);
        });

    connect(qnGlobalSettings, &QnGlobalSettings::updateNotificationsChanged, this,
        [this]()
        {
            setAutoUpdateCheckMode(qnGlobalSettings->isUpdateNotificationsEnabled());
        });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this,
        [this]()
        {
            if (m_widgetState != WidgetUpdateState::ready
                && m_widgetState != WidgetUpdateState::readyInstall
                && m_widgetState != WidgetUpdateState::downloading)
            {
                return;
            }

            NX_VERBOSE(this, "Cloud settings has been changed. We should repeat validation.");
            repeatUpdateValidation();
        });

    connect(m_stateTracker.get(), &PeerStateTracker::itemRemoved, this,
        [this](UpdateItemPtr item)
        {
            if (m_widgetState == WidgetUpdateState::downloading)
                m_stateTracker->removeFromTask(item->id);
            atServerConfigurationChanged(item);
        });

    connect(m_stateTracker.get(), &PeerStateTracker::itemAdded, this,
        [this](UpdateItemPtr item)
        {
            if (m_widgetState == WidgetUpdateState::downloading)
                m_stateTracker->addToTask(item->id);
            atServerConfigurationChanged(item);
        });

    connect(m_stateTracker.get(), &PeerStateTracker::itemOnlineStatusChanged, this,
        &MultiServerUpdatesWidget::atServerConfigurationChanged);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::startUpdateComplete,
        this, &MultiServerUpdatesWidget::atStartUpdateComplete);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::finishUpdateComplete,
        this, &MultiServerUpdatesWidget::atFinishUpdateComplete,
        Qt::ConnectionType::QueuedConnection);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::cancelUpdateComplete,
        this, &MultiServerUpdatesWidget::atCancelUpdateComplete);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::startInstallComplete,
        this, &MultiServerUpdatesWidget::atStartInstallComplete,
        Qt::ConnectionType::QueuedConnection);

    connect(qnGlobalSettings, &QnGlobalSettings::localSystemIdChanged, this,
        [this]()
        {
            auto systemId = helpers::currentSystemLocalId(resourcePool()->commonModule());
            NX_DEBUG(this, "localSystemId is changed to %1. Need to refresh server list", systemId);
            if (systemId.isNull())
            {
                NX_DEBUG(this, "localSystemIdChanged() we have disconnected. Detaching resource pool");
                m_stateTracker->setResourceFeed(nullptr);
                return;
            }
            else if (m_stateTracker && m_stateTracker->setResourceFeed(resourcePool()))
            {
                // We will be here when we connected to another system.
                // We should run update check again. This should fix VMS-13037.
                if (m_widgetState == WidgetUpdateState::initial && !m_updateCheck.valid())
                {
                    QString updateUrl = qnSettings->updateFeedUrl();
                    m_updateCheck = m_serverUpdateTool->checkLatestUpdate(updateUrl);
                }
            }
        });

    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            // This prevents widget from recalculating update report for each server removed from
            // the resource pool during disconnect.
            if (!user)
            {
                NX_DEBUG(this, "Disconnected from the system. Cleaning up current update context");
                // We will be here when we disconnect from the server.
                // So we can clear our state as well.
                clearUpdateInfo();
                setTargetState(WidgetUpdateState::initial, {});
                setUpdateSourceMode(UpdateSourceType::internet);
            }
        });

    setWarningStyle(ui->longUpdateWarning);

    ui->errorLabel->setText(QString());
    setWarningStyle(ui->errorLabel);

    ui->dayWarningBanner->setBackgroundRole(QPalette::AlternateBase);
    ui->dayWarningLabel->setForegroundRole(QPalette::Text);

    static_assert(kTooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningBanner->setVisible(false);
    ui->longUpdateWarning->setVisible(false);
    ui->browseUpdate->setVisible(false);

    ui->releaseNotesUrl->setText(QString("<a href='notes'>%1</a>").arg(tr("Release notes")));

    setWarningFrame(ui->releaseDescriptionLabel);
    ui->releaseDescriptionLabel->setOpenExternalLinks(true);
    ui->releaseDescriptionLabel->setText(QString());

    QTimer* updateTimer = new QTimer(this);
    updateTimer->setSingleShot(false);
    updateTimer->start(kAutoCheckIntervalMs);
    connect(updateTimer, &QTimer::timeout,
        this, &MultiServerUpdatesWidget::autoCheckForUpdates);

    m_longUpdateWarningTimer.reset(new QTimer(this));
    m_longUpdateWarningTimer->setInterval(kLongInstallWarningTimeout);
    m_longUpdateWarningTimer->setSingleShot(true);
    connect(m_longUpdateWarningTimer.get(), &QTimer::timeout,
        ui->longUpdateWarning, &QLabel::show);

    connect(qnGlobalSettings, &QnGlobalSettings::updateNotificationsChanged,
        this, &MultiServerUpdatesWidget::loadDataToUi);

    ui->manualDownloadButton->hide();
    ui->manualDownloadButton->setIcon(qnSkin->icon("text_buttons/download.png"));
    ui->manualDownloadButton->setForegroundRole(QPalette::WindowText);

    initDownloadActions();
    initDropdownActions();

    // Starting timer that tracks remote update state
    m_stateCheckTimer.reset(new QTimer(this));
    m_stateCheckTimer->setSingleShot(false);
    m_stateCheckTimer->start(1000);
    connect(m_stateCheckTimer.get(), &QTimer::timeout,
        this, &MultiServerUpdatesWidget::atUpdateCurrentState);

    const auto connection = commonModule()->ec2Connection();
    if (connection)
    {
        QnConnectionInfo connectionInfo = connection->connectionInfo();

        QnUuid serverId = QnUuid(connectionInfo.ecsGuid);
        nx::utils::Url serverUrl = connectionInfo.ecUrl;
        m_clientUpdateTool->setServerUrl(serverUrl, serverId);
        //m_clientUpdateTool->requestRemoteUpdateInfo();
    }
    // Force update when we open dialog.
    checkForInternetUpdates(/*initial=*/true);

    loadDataToUi();
}

MultiServerUpdatesWidget::~MultiServerUpdatesWidget()
{
}

void MultiServerUpdatesWidget::setAutoUpdateCheckMode(bool mode)
{
    if (mode)
    {
        ui->updateCheckMode->setText(tr("Checking for updates automatically"));
        ui->updateCheckMode->setIcon(qnSkin->icon("text_buttons/ok.png"));
    }
    else
    {
        ui->updateCheckMode->setText(tr("Check for updates"));
        ui->updateCheckMode->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    }

    if (mode != m_autoCheckUpdate)
    {
        m_autoCheckUpdate = mode;
        qnGlobalSettings->setUpdateNotificationsEnabled(mode);
        qnGlobalSettings->synchronizeNow();
    }
}

void MultiServerUpdatesWidget::initDropdownActions()
{
    m_selectUpdateTypeMenu.reset(new QMenu(this));
    m_selectUpdateTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceType::internet) + "...",
        [this]()
        {
            setUpdateSourceMode(UpdateSourceType::internet);
        });

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceType::internetSpecific) + "...",
        [this]()
        {
            // We are forcing this change to make appear dialog for picking a build.
            setUpdateSourceMode(UpdateSourceType::internetSpecific, true);
        });

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceType::file) + "...",
        [this]()
        {
            setUpdateSourceMode(UpdateSourceType::file);
        });

    ui->selectUpdateTypeButton->setMenu(m_selectUpdateTypeMenu.get());
    setUpdateSourceMode(UpdateSourceType::internet);

    m_autoCheckMenu.reset(new QMenu(this));
    m_autoCheckMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    m_autoCheckMenu->addAction(tr("Force check"), this, &MultiServerUpdatesWidget::checkForInternetUpdates);
    m_autoCheckMenu->addAction(tr("Disable Automatically Checking"),
        [this]()
        {
            setAutoUpdateCheckMode(false);
        });

    m_manualCheckMenu.reset(new QMenu(this));
    m_manualCheckMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    m_manualCheckMenu->addAction(tr("Check Once"), this, &MultiServerUpdatesWidget::checkForInternetUpdates);
    m_manualCheckMenu->addAction(tr("Check Automatically"),
        [this]()
        {
            setAutoUpdateCheckMode(true);
        });

    connect(ui->updateCheckMode, &QPushButton::clicked, this,
        [this]()
        {
            auto menu = (m_autoCheckUpdate ? m_autoCheckMenu : m_manualCheckMenu).get();
            menu->exec(ui->updateCheckMode->mapToGlobal(
                ui->updateCheckMode->rect().bottomLeft() + QPoint(0, 1)));
            ui->updateCheckMode->update();
        });

    setAutoUpdateCheckMode(qnGlobalSettings->isUpdateNotificationsEnabled());
}

void MultiServerUpdatesWidget::initDownloadActions()
{
    auto downloadLinkMenu = new QMenu(this);
    downloadLinkMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    downloadLinkMenu->addAction(tr("Download in External Browser"),
        [this]()
        {
            QSet<QnUuid> targets = m_stateTracker->allPeers();
            auto url = generateUpdatePackageUrl(
                commonModule()->engineVersion(),
                m_updateInfo, targets,
                resourcePool()).toString();

            QDesktopServices::openUrl(url);
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            QSet<QnUuid> targets = m_stateTracker->allPeers();
            auto url = generateUpdatePackageUrl(
                commonModule()->engineVersion(),
                m_updateInfo,
                targets,
                resourcePool()).toString();
            qApp->clipboard()->setText(url);

            ui->linkCopiedWidget->show();
            fadeWidget(ui->linkCopiedWidget, 1.0, 0.0, kLinkCopiedMessageTimeoutMs, 1.0,
                [this]()
                {
                    ui->linkCopiedWidget->setGraphicsEffect(nullptr);
                    ui->linkCopiedWidget->hide();
                });
        });

    connect(ui->manualDownloadButton, &QPushButton::clicked, this,
        [this, downloadLinkMenu]()
        {
            downloadLinkMenu->exec(ui->manualDownloadButton->mapToGlobal(
                ui->manualDownloadButton->rect().bottomLeft() + QPoint(0, 1)));

            ui->manualDownloadButton->update();
        });
}

MultiServerUpdatesWidget::VersionReport MultiServerUpdatesWidget::calculateUpdateVersionReport(
    const nx::update::UpdateContents& contents, QnUuid clientId)
{
    VersionReport report;

    using Error = nx::update::InformationError;
    using SourceType = nx::update::UpdateSourceType;

    bool validUpdate = contents.isValidToInstall();
    auto source = contents.sourceType;

    QString internalError = nx::update::toString(contents.error);
    // We have different error messages for each update source. So we should check
    // every combination of update source and nx::update::InformationError values.
    if (contents.alreadyInstalled && source != SourceType::internet
        && contents.error != Error::incompatibleVersion)
    {
        report.versionMode = VersionReport::VersionMode::build;
        report.statusHighlight = VersionReport::HighlightMode::regular;
        report.statusMessages << tr("You have already installed this version.");
        report.hasLatestVersion = true;
    }
    else if (contents.error == Error::noError)
    {
        report.version = contents.info.version;
        return report;
    }
    else if (!validUpdate)
    {
        report.versionHighlight = VersionReport::HighlightMode::red;
        report.statusHighlight = VersionReport::HighlightMode::red;
        // Note: it is possible to have multiple problems. Some problems can be tracked by
        // both error code and some error flag. So some error statuses are filled separately
        // from this switch.
        switch(contents.error)
        {
            case Error::noError:
                // We should not be here.
                break;
            case Error::networkError:
                // Unable to check update from the internet.
                report.statusMessages << tr("Unable to check updates on the internet");
                report.versionMode = VersionReport::VersionMode::empty;
                report.versionHighlight = VersionReport::HighlightMode::regular;
                break;
            case Error::httpError:
                NX_ASSERT(source == SourceType::internet || source == SourceType::internetSpecific);
                if (source == SourceType::internetSpecific)
                {
                    report.versionMode = VersionReport::VersionMode::build;
                    report.statusMessages << tr("Build not found");
                }
                else
                {
                    report.versionMode = VersionReport::VersionMode::empty;
                    report.statusMessages << tr("Unable to check updates on the internet");
                }
                break;
            case Error::incompatibleParser:
                NX_ERROR(NX_SCOPE_TAG, "A proper update description file is not found.");
                [[fallthrough]];
            case Error::jsonError:
                if (source == SourceType::file)
                    report.statusMessages << tr("Cannot update from the selected file");
                else
                    report.statusMessages << tr("Invalid update information");
                break;
            case Error::incompatibleVersion:
                report.statusMessages << tr("Downgrade to earlier versions is not possible");
                break;
            case Error::notFoundError:
                // No update
                report.statusMessages << tr("Update file is not found");
                break;
            case Error::incompatibleCloudHost:
                // Cloud host is incompatible. Error message is filled later.
                report.versionHighlight = VersionReport::HighlightMode::regular;
                break;
            case Error::noNewVersion:
                // We have most recent version for this build.
                report.hasLatestVersion = true;
                break;
            case Error::brokenPackageError:
                report.statusMessages << tr("Upgrade package is broken");
                break;
            case Error::missingPackageError:
            {
                QStringList packageErrors;
                if (auto missing = contents.missingUpdate.size())
                {
                    if (contents.missingUpdate.contains(clientId))
                    {
                        if (missing > 1)
                        {
                            // Decrementing 'missing' value to remove client package from the counter.
                            packageErrors << tr("Missing update package for the client and %n servers",
                                "", missing - 1);
                        }
                        else
                        {
                            packageErrors << tr("Missing update package for the client");
                        }
                    }
                    else
                    {
                        packageErrors << tr("Missing update package for some servers");
                    }
                }

                if (auto unsupported = contents.unsuportedSystemsReport.size())
                {
                    if (contents.unsuportedSystemsReport.contains(clientId) && unsupported == 1)
                    {
                        packageErrors << tr("OS version of the client is no longer supported. "
                            "Please update its OS to a supported version.");
                    }
                    else
                    {
                        packageErrors << tr("OS versions of some components are no longer supported. "
                            "Please remove them from the System or update their OS to a supported version.");
                    }
                }

                report.versionHighlight = VersionReport::HighlightMode::bright;
                report.statusMessages << packageErrors;
                break;
            }

            case Error::serverConnectionError:
                // We do not report anything for this case.
                break;
        }
    }

    if (report.versionMode == VersionReport::VersionMode::empty)
        report.version = kNoVersionNumberText;
    else if (contents.info.version.isEmpty())
        report.version = contents.changeset;
    else
        report.version = contents.info.version;

    if (!contents.cloudIsCompatible)
    {
        report.statusMessages << tr("Incompatible %1 instance. To update disconnect System from %1 first.",
            "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
            .arg(nx::network::AppInfo::cloudName());
    }

    return report;
}

bool MultiServerUpdatesWidget::checkSpaceRequirements(
    const nx::update::UpdateContents& contents) const
{
    bool checkClient = m_clientUpdateTool->shouldInstallThis(contents);
    auto spaceForManualPackages = contents.getClientSpaceRequirements(checkClient);
    if (spaceForManualPackages > 0)
    {
        auto downloadDir = m_serverUpdateTool->getDownloadDir();
        auto spaceAvailable = m_serverUpdateTool->getAvailableSpace();
        return spaceForManualPackages < spaceAvailable;
    }
    return true;
}

void MultiServerUpdatesWidget::setUpdateTarget(
    const nx::update::UpdateContents& contents, bool activeUpdate)
{
    NX_VERBOSE(this, "setUpdateTarget(%1)", contents.getVersion());
    m_updateInfo = contents;

    m_stateTracker->clearVerificationErrors();

    if (!m_updateInfo.unsuportedSystemsReport.empty())
        m_stateTracker->setVerificationError(contents.unsuportedSystemsReport);

    if (!m_updateInfo.missingUpdate.empty())
    {
        m_stateTracker->setVerificationError(
            contents.missingUpdate, tr("No update package available"));
    }

    auto clientId = clientPeerId();
    auto report = calculateUpdateVersionReport(m_updateInfo, clientId);
    m_updateReport = report;
    m_stateTracker->setUpdateTarget(contents.getVersion());
    m_forceUiStateUpdate = true;
    m_updateRemoteStateChanged = true;
    // TODO: We should collect all these changes to a separate state-structure.
    // TODO: We should split state flags more consistenly.
}

QnUuid MultiServerUpdatesWidget::clientPeerId() const
{
    return m_stateTracker->getClientPeerId(commonModule());
}

void MultiServerUpdatesWidget::atUpdateCurrentState()
{
    NX_ASSERT(m_serverUpdateTool);

    if (isHidden())
        return;

    // We poll all our tools for new information. Then we update UI if there are any changes
    if (m_updateCheck.valid()
        && m_updateCheck.wait_for(kWaitForUpdateCheckFuture) == std::future_status::ready)
    {
        auto checkResponse = m_updateCheck.get();
        NX_VERBOSE(this) << "atUpdateCurrentState got update info:"
            << checkResponse.info.version
            << "from" << checkResponse.source;
        if (checkResponse.sourceType == nx::update::UpdateSourceType::mediaservers)
        {
            NX_WARNING(this,
                "atUpdateCurrentState() this is the data from /ec2/updateInformation.");
        }

        m_serverUpdateTool->verifyUpdateManifest(checkResponse,
            m_clientUpdateTool->getInstalledClientVersions());
        if (!hasActiveUpdate())
        {
            if (m_updateInfo.preferOtherUpdate(checkResponse))
            {
                setUpdateTarget(checkResponse, /*activeUpdate=*/false);
            }
            else
            {
                NX_VERBOSE(this, "atUpdateCurrentState() current update info with version='%1' "
                    "is better", m_updateInfo.info.version);
            }
        }
        else
        {
            NX_VERBOSE(this, "atUpdateCurrentState() got update version='%1', but we are already "
                "updating to version='%2' in state=%3. Ignoring it.", checkResponse.info.version,
                m_updateInfo.info.version, toString(m_widgetState));
        }
    }

    // Maybe we should not call it right here.
    m_serverUpdateTool->requestRemoteUpdateStateAsync();

    processRemoteChanges();

    // TODO: we should invoke syncProgress only once inside loadDataToUi()
    if (stateHasProgress(m_widgetState))
        syncProgress();
    if (hasPendingUiChanges())
        loadDataToUi();

    syncDebugInfoToUi();
}

bool MultiServerUpdatesWidget::hasPendingUiChanges() const
{
    return m_updateRemoteStateChanged
        || m_forceUiStateUpdate
        || m_updateSourceMode.changed()
        || m_controlPanelState.changed()
        || m_widgetState.changed()
        || m_statusColumnMode.changed();
}

void MultiServerUpdatesWidget::atCheckInstallState()
{
    NX_ASSERT(m_serverUpdateTool);
    if (!m_serverUpdateTool)
        return;

    if (m_widgetState == WidgetUpdateState::installing
        || m_widgetState == WidgetUpdateState::installingStalled)
    {
        NX_ASSERT(m_installCheckTimer);
        m_serverUpdateTool->requestModuleInformation();
        m_installCheckTimer->start(kPeriodForCheckingInstallStatus);
    }
    else
    {
        NX_VERBOSE(this, "atCheckInstallState() - stopping timer for checking install status");
        m_installCheckTimer.reset();
    }
}

bool MultiServerUpdatesWidget::tryClose(bool /*force*/)
{
    // Should we cancell all pending ations on exit?
    //if(m_updateTool)
    //    m_updateTool->cancelAll();
    return true;
}

void MultiServerUpdatesWidget::forcedUpdate()
{
    NX_VERBOSE(this) << "forcedUpdate()";
    checkForInternetUpdates();
}

void MultiServerUpdatesWidget::clearUpdateInfo()
{
    NX_INFO(this, "clearUpdateInfo()");
    m_updateReport = VersionReport();
    m_updateInfo = nx::update::UpdateContents();
    m_updatesModel->setUpdateTarget(nx::utils::SoftwareVersion());
    m_stateTracker->clearVerificationErrors();
    m_forceUiStateUpdate = true;
    m_updateCheck = std::future<nx::update::UpdateContents>();
}

void MultiServerUpdatesWidget::pickLocalFile()
{
    auto options = QnCustomFileDialog::fileDialogOptions();
    QString caption = tr("Select Update File...");
    QString filter = QnCustomFileDialog::createFilter(tr("Update Files"), "zip");
    QString fileName = QFileDialog::getOpenFileName(this, caption, {}, filter, nullptr, options);

    if (fileName.isEmpty())
        return;

    m_updateSourceMode = UpdateSourceType::file;
    m_forceUiStateUpdate = true;

    clearUpdateInfo();
    m_updateCheck = m_serverUpdateTool->checkUpdateFromFile(fileName);

    loadDataToUi();
}

void MultiServerUpdatesWidget::pickSpecificBuild()
{
    QnBuildNumberDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_updateSourceMode = UpdateSourceType::internetSpecific;
    m_forceUiStateUpdate = true;

    clearUpdateInfo();
    QString updateUrl = qnSettings->updateFeedUrl();
    m_updateCheck = m_serverUpdateTool->checkSpecificChangeset(
        updateUrl,
        dialog.changeset());
    loadDataToUi();
}

void MultiServerUpdatesWidget::checkForInternetUpdates(bool initial)
{
    if (m_updateSourceMode != UpdateSourceType::internet)
        return;

    if (!initial && !isVisible())
        return;

    // No need to check for updates if we are already installing something.
    if (m_widgetState != WidgetUpdateState::initial && m_widgetState != WidgetUpdateState::ready)
        return;

    if (initial && !m_updateCheck.valid())
    {
        // WorkbenchUpdateWatcher checks for updates periodically. We could reuse its update
        // info, if there is any.
        auto watcher = context()->instance<nx::vms::client::desktop::WorkbenchUpdateWatcher>();
        auto contents = watcher->getUpdateContents();
        auto installedVersions = m_clientUpdateTool->getInstalledClientVersions();
        if (!contents.isEmpty()
            && m_serverUpdateTool->verifyUpdateManifest(contents, installedVersions))
        {
            // Widget FSM uses future<UpdateContents> while it spins around 'initial'->'ready'.
            // So the easiest way to advance it towards 'ready' state is by providing another
            // future to await.
            NX_INFO(this, "checkForInternetUpdates(%1) - picking update information from "
                "WorkbenchUpdateWatcher", initial);
            std::promise<nx::update::UpdateContents> promise;
            m_updateCheck = promise.get_future();
            promise.set_value(contents);
        }
    }

    if (!m_updateCheck.valid())
    {
        clearUpdateInfo();
        QString updateUrl = qnSettings->updateFeedUrl();
        m_updateCheck = m_serverUpdateTool->checkLatestUpdate(updateUrl);
        m_updateReport.compareAndSet(m_updateReport->checking, isChecking());
        // We have changed 'isChecking' here.
        syncUpdateCheckToUi();
    }
}

void MultiServerUpdatesWidget::setUpdateSourceMode(UpdateSourceType mode, bool force)
{
    if (m_updateSourceMode == mode && !force)
        return;

    switch(mode)
    {
        case UpdateSourceType::internet:
            m_updateSourceMode = mode;
            m_forceUiStateUpdate = true;
            clearUpdateInfo();
            checkForInternetUpdates();
            loadDataToUi();
            break;
        case UpdateSourceType::internetSpecific:
            pickSpecificBuild();
            break;
        case UpdateSourceType::file:
            pickLocalFile();
            break;
        case UpdateSourceType::mediaservers:
            // Should not be here.
            NX_ASSERT(false);
            break;
    }
}

void MultiServerUpdatesWidget::atStartUpdateAction()
{
    // Clicked 'Download' button. Should start download process here.
    NX_ASSERT(m_serverUpdateTool);

    if (m_widgetState == WidgetUpdateState::readyInstall)
    {
        auto targets = m_stateTracker->peersInState(StatusCode::readyToInstall);
        if (targets.empty() && !m_clientUpdateTool->hasUpdate())
        {
            NX_WARNING(this) << "atStartUpdateAction() - no peer can install anything";
            return;
        }

        NX_INFO(this, "atStartUpdateAction() - starting installation for %1", targets);
        setTargetState(WidgetUpdateState::startingInstall, targets);
    }
    else if (m_widgetState == WidgetUpdateState::ready && m_updateInfo.isValidToInstall())
    {
        int acceptedEula = qnSettings->acceptedEulaVersion();
        int newEula = m_updateInfo.info.eulaVersion;
        const bool showEula = acceptedEula < newEula;

        if (showEula && !EulaDialog::acceptEulaHtml(m_updateInfo.info.eula, newEula, mainWindowWidget()))
            return;

        auto targets = m_stateTracker->allPeers();

        // Remove the servers which should not be updated. These are the servers with more recent
        // version. So we should not track update progress for them.
        targets.subtract(m_updateInfo.ignorePeers);

        auto offlineServers = m_stateTracker->offlineServers();
        targets.subtract(offlineServers);
        if (!offlineServers.empty())
        {
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Warning);

            if (targets.empty())
            {
                messageBox->setText(tr("There are no online servers to update."));
                messageBox->addButton(QDialogButtonBox::Ok);
                messageBox->exec();
                return;
            }

            messageBox->setText(tr("Some servers are offline and will not be updated. Skip them?"));
            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(offlineServers));
            messageBox->addCustomButton(QnMessageBoxCustomButton::Skip,
                QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);
            auto cancel = messageBox->addButton(QDialogButtonBox::Cancel);
            messageBox->exec();
            auto clicked = messageBox->clickedButton();
            if (clicked == cancel)
                return;
        }

        auto incompatible = m_stateTracker->legacyServers();
        if (!incompatible.empty())
        {
            /*
            nx::utils::SoftwareVersion version = m_targetVersion;
            m_updatesTool->startOnlineClientUpdate(incompatible, m_targetVersion, false);
            // Run compatibility update
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Warning);
            messageBox->setText(tr("Some servers have incompatible versions and will not be updated"));
            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(incompatible));
            auto ok = messageBox->addButton(QDialogButtonBox::Ok);
            messageBox->setEscapeButton(ok);
            messageBox->exec();*/

            targets.subtract(incompatible);
        }

        // We will not track client state during download. But we still may restart to the new
        // version.
        if (m_clientUpdateTool->isVersionInstalled(m_updateInfo.getVersion())
            || !m_updateInfo.needClientUpdate)
        {
            targets.remove(clientPeerId());
        }

        m_stateTracker->setUpdateTarget(m_updateInfo.getVersion());
        m_stateTracker->markStatusUnknown(targets);

        setTargetState(WidgetUpdateState::startingDownload, targets);

        NX_INFO(this, "atStartUpdateAction() - sending 'download' command to peers %1", targets);
        m_serverUpdateTool->requestStartUpdate(m_updateInfo.info, targets);
        m_forceUiStateUpdate = true;
    }
    else
    {
        NX_WARNING(this, "atStartUpdateAction() - invalid widget state for download command %1",
            toString(m_widgetState));
    }

    if (hasPendingUiChanges())
        loadDataToUi();
}

bool MultiServerUpdatesWidget::atCancelCurrentAction()
{
    closePanelNotifications();

    auto showCancelDialog =
        [this]() -> bool
        {
            // This will be used at 'downloading', 'readyToInstall' and 'pushing' states.
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Question);
            messageBox->setText(tr("Cancel update and delete all downloaded data?"));
            messageBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
            messageBox->setDefaultButton(QDialogButtonBox::Yes, Qn::ButtonAccent::Warning);
            return messageBox->exec() == QDialogButtonBox::Yes;
        };
    // Cancel all the downloading.
    if (m_widgetState == WidgetUpdateState::downloading
        || m_widgetState == WidgetUpdateState::startingDownload)
    {
        if (showCancelDialog())
        {
            NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState);
            m_serverUpdateTool->stopUpload();
            setTargetState(WidgetUpdateState::cancelingDownload, {});
            m_serverUpdateTool->requestStopAction();
        }
    }
    else if (m_widgetState == WidgetUpdateState::installingStalled)
    {
        // Should send 'cancel' command to all the servers?
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState);

        auto serversToCancel = m_stateTracker->peersInstalling();
        auto peersIssued = m_stateTracker->peersIssued();

        QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
        // 3. All other cases. Some servers have failed
        messageBox->setIcon(QnMessageBoxIcon::Question);
        messageBox->setText(
            tr("Some servers have not completed the update process. Finish it anyway?"));
        messageBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
        messageBox->setDefaultButton(QDialogButtonBox::Yes, Qn::ButtonAccent::Warning);

        if (messageBox->exec() == QDialogButtonBox::Yes)
        {
            setTargetState(WidgetUpdateState::finishingInstall, peersIssued);
            //m_serverUpdateTool->requestFinishUpdate(/*skipActivePeers=*/true);

        }
    }
    else if (m_widgetState == WidgetUpdateState::complete)
    {
        // Should send 'cancel' command to all the servers?
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState);
        setTargetState(WidgetUpdateState::finishingInstall, {});
        //m_serverUpdateTool->requestFinishUpdate(/*skipActivePeers=*/false);
    }
    else if (m_widgetState == WidgetUpdateState::readyInstall)
    {
        if (showCancelDialog())
        {
            NX_VERBOSE(this) << "atCancelCurrentAction() at" << toString(m_widgetState);
            m_serverUpdateTool->requestStopAction();
            setTargetState(WidgetUpdateState::cancelingReadyInstall, {});
        }
    }
    else
    {
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState) << ": not implemented";
        clearUpdateInfo();
        setUpdateSourceMode(UpdateSourceType::internet);
        checkForInternetUpdates();
        return false;
    }

    if (hasPendingUiChanges())
        loadDataToUi();

    // Spec says that we can not cancel anything when we began installing stuff.
    return true;
}

void MultiServerUpdatesWidget::atStartUpdateComplete(bool success, const QString& error)
{
    if (m_widgetState == WidgetUpdateState::startingDownload)
    {
        if (success)
        {
            // TODO: We should merge remote update info and the current one.
            auto targets = m_stateTracker->peersIssued();
            setTargetState(WidgetUpdateState::downloading, targets);
            // We always run install commands for client. Though clientUpdateTool state can
            // fall through to 'readyRestart' or 'complete' state.
            m_clientUpdateTool->setUpdateTarget(m_updateInfo);
            // Start download process.
            // Move to state 'Downloading'
            // At this state should check remote state untill everything is complete.
            // TODO: We have the case when all mediaservers have installed update.
            if (!m_updateInfo.manualPackages.empty())
                m_serverUpdateTool->startManualDownloads(m_updateInfo);

            if (m_serverStatusCheck.valid())
                NX_ERROR(this, "atStartUpdateComplete() - m_serverStatusCheck is not empty. ");

            m_serverStatusCheck = m_serverUpdateTool->requestRemoteUpdateState();
        }
        else
        {
            auto messageBox = std::make_unique<QnSessionAwareMessageBox>(this);
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to start update"));
            messageBox->setInformativeText(error);
            messageBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Retry);
            messageBox->setDefaultButton(QDialogButtonBox::Ok);
            if (messageBox->exec() == QDialogButtonBox::Retry)
            {
                auto targets = m_stateTracker->peersIssued();
                m_serverUpdateTool->requestStartUpdate(m_updateInfo.info, targets);
            }
            else
            {
                setTargetState(WidgetUpdateState::ready, {});
            }
        }
    }
}

void MultiServerUpdatesWidget::atCancelUpdateComplete(bool success, const QString& error)
{
    if (m_widgetState == WidgetUpdateState::cancelingDownload
        || m_widgetState == WidgetUpdateState::cancelingReadyInstall)
    {
        m_clientUpdateTool->resetState();
        if (success)
        {
            repeatUpdateValidation();
            setTargetState(WidgetUpdateState::ready, {});
        }
        else
        {
            NX_ERROR(this, "atCancelUpdateComplete(%1) - %2", success, error);
            QScopedPointer<QnMessageBox> messageBox(new QnMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to cancel update"));
            messageBox->setInformativeText(error);
            messageBox->setStandardButtons(QDialogButtonBox::Ok);
            messageBox->exec();
            QSet<QnUuid> peersIssued = m_stateTracker->peersIssued();
            if (m_widgetState == WidgetUpdateState::cancelingDownload)
                setTargetState(WidgetUpdateState::downloading, peersIssued);
            else
                setTargetState(WidgetUpdateState::readyInstall, peersIssued);
        }
    }

    if (hasPendingUiChanges())
        loadDataToUi();
}

void MultiServerUpdatesWidget::atStartInstallComplete(bool success, const QString& error)
{
    if (m_widgetState == WidgetUpdateState::startingInstall)
    {
        if (success)
        {
            auto targets = m_stateTracker->peersIssued();
            NX_INFO(this, "atStartInstallComplete(success) targets = %1", targets);
            setTargetState(WidgetUpdateState::installing, targets);
        }
        else
        {
            QScopedPointer<QnMessageBox> messageBox(new QnMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to start installation"));
            messageBox->setInformativeText(error);
            messageBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Retry);
            messageBox->setDefaultButton(QDialogButtonBox::Ok);
            QSet<QnUuid> servers = m_stateTracker->peersIssued();
            if (messageBox->exec() == QDialogButtonBox::Retry)
            {
                servers.remove(clientPeerId());
                m_serverUpdateTool->requestInstallAction(servers);
            }
            else
            {
                setTargetState(WidgetUpdateState::readyInstall, servers);
            }
        }
    }
    else
    {
        // We should not be here.
        NX_ASSERT(false);
    }

    if (hasPendingUiChanges())
        loadDataToUi();
}

void MultiServerUpdatesWidget::atFinishUpdateComplete(bool success, const QString& error)
{
    if (m_widgetState == WidgetUpdateState::finishingInstall)
    {
        if (success)
        {
            m_stateTracker->processInstallTaskSet();
            auto peersComplete = m_stateTracker->peersComplete();
            auto peersFailed = m_stateTracker->peersFailed();

            if (!peersComplete.empty())
            {
                NX_INFO(this, "atFinishUpdateComplete() - installation is complete");
                setTargetState(WidgetUpdateState::complete, peersComplete);
                // Forcing UI to redraw before we show a dialog.
                loadDataToUi();

                auto complete = peersComplete;
                QScopedPointer<QnMessageBox> messageBox(new QnMessageBox(this));
                // 1. Everything is complete
                messageBox->setIcon(QnMessageBoxIcon::Success);

                if (peersFailed.empty())
                {
                    messageBox->setText(tr("Update completed"));
                }
                else
                {
                    NX_ERROR(this,
                        "atFinishUpdateComplete() - peers %1 have failed to install update",
                        peersFailed);
                    messageBox->setText(
                        tr("Update completed, but some components have failed an update"));
                    injectResourceList(*messageBox, resourcePool()->getResourcesByIds(peersFailed));
                }

                QStringList informativeText;
                if (m_clientUpdateTool->shouldRestartTo(m_updateInfo.getVersion()))
                {
                    QString appName = QnClientAppInfo::applicationDisplayName();
                    if (peersFailed.contains(clientPeerId()))
                    {
                        informativeText += tr(
                            "Please update %1 manually using an installation package.").arg(appName);
                    }
                    else
                    {
                        informativeText += tr("%1 will be restarted to the updated version.").arg(
                            appName);
                    }
                }

                if (!informativeText.isEmpty())
                    messageBox->setInformativeText(informativeText.join("\n"));

                messageBox->addButton(tr("OK"),
                    QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
                messageBox->exec();

                bool shouldRestartClient = m_clientUpdateTool->hasUpdate()
                    && m_clientUpdateTool->shouldRestartTo(m_updateInfo.getVersion());

                completeClientInstallation(shouldRestartClient);
            }
            // No servers have installed updates
            else if (peersComplete.empty() && !peersFailed.empty())
            {
                NX_ERROR(this, "atFinishUpdateComplete() - installation has failed completely");
                QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
                // 1. Everything is complete
                messageBox->setIcon(QnMessageBoxIcon::Critical);
                messageBox->setText(tr("There was an error while installing updates:"));

                PeerStateTracker::ErrorReport report;
                m_stateTracker->getErrorReport(report);

                injectResourceList(*messageBox, resourcePool()->getResourcesByIds(report.peers));
                QString text =  htmlParagraph(report.message);
                text += htmlParagraph(tr("If the problem persists, please contact Customer Support."));
                messageBox->setInformativeText(text);
                auto installNow = messageBox->addButton(tr("OK"),
                    QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
                messageBox->setEscapeButton(installNow);
                messageBox->exec();
                setTargetState(WidgetUpdateState::initial);
                setUpdateSourceMode(UpdateSourceType::internet);
                loadDataToUi();
            }
            else
            {
                NX_ERROR(this, "atFinishUpdateComplete() - unhandled state");
            }
        }
        else
        {
            NX_ERROR(this, "atFinishUpdateComplete(%1) - %2", success, error);
            auto targets = m_stateTracker->peersIssued() + m_stateTracker->peersInstalling();
            setTargetState(WidgetUpdateState::installingStalled, targets);
        }
    }
    else
    {
        // We should not be here.
        NX_ASSERT(false);
    }

    if (hasPendingUiChanges())
        loadDataToUi();
}

void MultiServerUpdatesWidget::repeatUpdateValidation()
{
    if (!m_updateInfo.isEmpty())
    {
        NX_INFO(this, "repeatUpdateValidation() - recalculating update report");
        auto installedVersions = m_clientUpdateTool->getInstalledClientVersions();
        if (m_updateInfo.error != nx::update::InformationError::httpError
            || m_updateInfo.error != nx::update::InformationError::networkError)
        {
            m_updateInfo.error = nx::update::InformationError::noError;
        }

        m_serverUpdateTool->verifyUpdateManifest(m_updateInfo, installedVersions);
        setUpdateTarget(m_updateInfo,
            m_updateInfo.sourceType != nx::update::UpdateSourceType::mediaservers);
    }
    else
    {
        NX_INFO(this, "repeatUpdateValidation() - update info is completely empty. Nothing to recalculate");
    }

    if (m_forceUiStateUpdate)
        loadDataToUi();
}

void MultiServerUpdatesWidget::atServerPackageDownloaded(const nx::update::Package& package)
{
    if (m_widgetState == WidgetUpdateState::downloading)
    {
        NX_INFO(this)
            << "atServerPackageDownloaded() - downloaded server package"
            << package.file;

        m_serverUpdateTool->uploadPackage(package, m_serverUpdateTool->getDownloadDir());
    }
    else
    {
        NX_INFO(this)
            << "atServerPackageDownloaded() - download server package"
            << package.file << "and widget is not in downloading state";
    }
}

void MultiServerUpdatesWidget::atServerPackageDownloadFailed(
    const nx::update::Package& package,
    const QString& error)
{
    // This handler is used when client downloads packages for the servers without internet.
    if (m_widgetState == WidgetUpdateState::downloading)
    {
        if (m_serverUpdateTool->hasInitiatedThisUpdate())
        {
            NX_ERROR(this,
                "atServerPackageDownloadFailed() - failed to download server package \"%1\", "
                "error: %2", package.file, error);
            m_stateTracker->setTaskError(package.targets, "OfflineDownloadError");
        }
        else
        {
            NX_WARNING(this,
                "atServerPackageDownloadFailed() - failed to download server package \"%1\", "
                "error: %2. This client can ignore this problem.", package.file, error);
        }
    }
    else
    {
        NX_INFO(this,
            "atServerPackageDownloadFailed() - failed to download server package \"%1\" "
            "and widget is not in downloading state. Error: %2", package.file, error);
    }
}

void MultiServerUpdatesWidget::atServerConfigurationChanged(std::shared_ptr<UpdateItem> item)
{
    if (m_widgetState != WidgetUpdateState::ready
        && m_widgetState != WidgetUpdateState::readyInstall
        && m_widgetState != WidgetUpdateState::downloading)
    {
        return;
    }
    /* Possible changes:
     *  - server goes offline. We should wait it a bit if server was in readyInstall
     *  - server goes online. We should repeat verification
     *  - server was removed. We should check if we need to continue update.
     *  - server was added. We should repeat verification
     */

    if (!item->offline
        || !item->verificationMessage.isEmpty()
        || m_updateInfo.peersWithUpdate.contains(item->id))
    {
        // TODO: Make more conservative check: only check if server goes online, or if
        // server has errors and goes offline.
        NX_VERBOSE(this,
           "peer %1 has changed online status. We should repeat validation.", item->id);
        repeatUpdateValidation();
    }
    else if (item->offline)
    {
        m_updateRemoteStateChanged = true;
    }

    // TODO: We need task sets to be processed here and then to call loadDataToUi();
    atUpdateCurrentState();
}

ServerUpdateTool::ProgressInfo MultiServerUpdatesWidget::calculateActionProgress() const
{
    ServerUpdateTool::ProgressInfo result;
    if (m_widgetState == WidgetUpdateState::downloading)
    {
        auto peersIssued = m_stateTracker->peersIssued();
        auto peersActive = m_stateTracker->peersActive();
        // Note: we can get here right after we clicked 'download' but before
        // we got an update from /ec2/updateStatus. Most servers will be in 'idle' state.
        // We even can get a stale callback from /ec2/updateStatus, with data actual to
        // the moment right before we pressed 'Download'.
        // It will be very troublesome to properly wait for updated /ec2/updateStatus.
        for (const auto& id: peersIssued)
        {
            auto item = m_stateTracker->findItemById(id);
            if (!item)
                continue;
            if (item->installed && peersIssued.contains(id))
            {
                result.current += 100;
                ++result.done;
            }
            else
            {
                switch (item->state)
                {
                    case nx::update::Status::Code::idle:
                        // This stage has zero progress.
                        ++result.active;
                        break;
                    case nx::update::Status::Code::downloading:
                        result.current += item->progress;
                        ++result.active;
                        break;
                    case nx::update::Status::Code::error:
                    case nx::update::Status::Code::preparing:
                    case nx::update::Status::Code::readyToInstall:
                        result.current += 100;
                        ++result.done;
                        break;
                    case nx::update::Status::Code::latestUpdateInstalled:
                        if (peersIssued.contains(id))
                        {
                            result.current += 100;
                            ++result.done;
                        }
                        break;
                    case nx::update::Status::Code::offline:
                        break;
                }
            }
        }

        // We sum up individual progress from every server and adjust maximum progress as well.
        // We could get 147 of 200 percents done. ProgressBar works fine with it.
        result.max += 100 * peersIssued.size();
        result.downloadingServers = !peersActive.empty();

        if (m_serverUpdateTool->hasManualDownloads())
            m_serverUpdateTool->calculateManualDownloadProgress(result);

        if (m_clientUpdateTool->hasUpdate())
            result.downloadingClient = !m_clientUpdateTool->isDownloadComplete();
    }
    else if (m_widgetState == WidgetUpdateState::installing
        || m_widgetState == WidgetUpdateState::installingStalled)
    {
        // We will show simplified progress for this stage.
        auto peersActive = m_stateTracker->peersInstalling();
        result.installingServers = !peersActive.empty();
        result.current = 0;
        result.max = 0;
        if (m_clientUpdateTool->hasUpdate())
        {
            bool complete = m_clientUpdateTool->getState() == ClientUpdateTool::State::complete;
            result.installingClient = !complete;
        }
    }

    return result;
}

void MultiServerUpdatesWidget::processInitialState()
{
    if (!isVisible())
        return;

    if (!m_updateCheck.valid())
    {
        clearUpdateInfo();
        QString updateUrl = qnSettings->updateFeedUrl();
        m_updateCheck = m_serverUpdateTool->checkLatestUpdate(updateUrl);
    }

    if (!m_serverUpdateCheck.valid())
        m_serverUpdateCheck = m_serverUpdateTool->checkMediaserverUpdateInfo();

    if (!m_serverStatusCheck.valid())
        m_serverStatusCheck = m_serverUpdateTool->requestRemoteUpdateState();

    m_offlineUpdateCheck = m_serverUpdateTool->takeUpdateCheckFromFile();

    setTargetState(WidgetUpdateState::initialCheck, {});
    // We have changed widgetState and checking. Should update Ui.
    syncUpdateCheckToUi();
}

void MultiServerUpdatesWidget::processInitialCheckState()
{
    if (!m_serverUpdateCheck.valid())
    {
        NX_ERROR(this, "processInitialCheckState() - we were not waiting for "
            "/ec2/updateInformation for some reason. Reverting to initial state");
        setTargetState(WidgetUpdateState::initial, {});
        return;
    }

    bool mediaserverUpdateCheckReady =
        m_serverUpdateCheck.wait_for(kWaitForUpdateCheckFuture) == std::future_status::ready;
    bool mediaserverStatusCheckReady = m_serverStatusCheck.valid()
        && m_serverStatusCheck.wait_for(kWaitForUpdateCheckFuture) == std::future_status::ready;

    bool offlineCheckReady = !m_offlineUpdateCheck.valid()
        || m_offlineUpdateCheck.wait_for(kWaitForUpdateCheckFuture) == std::future_status::ready;

    if (mediaserverUpdateCheckReady && mediaserverStatusCheckReady && offlineCheckReady)
    {
        // TODO: We should invent a method for testing this.
        auto updateInfo = m_serverUpdateCheck.get();
        auto serverStatus = m_serverStatusCheck.get();

        // Update info for offline update package is already verified.
        if (m_offlineUpdateCheck.valid())
        {
            auto offlineUpdateInfo = m_offlineUpdateCheck.get();
            if (!offlineUpdateInfo.isEmpty())
            {
                NX_INFO(this,
                    "processInitialCheckState() - picking update contents from offline update.");
                // Note: mediaservers can have no active update right now
                updateInfo = offlineUpdateInfo;
            }
            else
            {
                NX_DEBUG(this, "processInitialCheckState() - offline update contents are empty.");
            }
        }

        ServerUpdateTool::RemoteStatus remoteStatus;
        m_serverUpdateTool->getServersStatusChanges(remoteStatus);
        m_stateTracker->setUpdateStatus(serverStatus);
        m_stateTracker->processUnknownStates();

        /*
         * There are two distinct situations:
         *  1. This is the client, that has initiated update process.
         *  2. This is 'other' client, that have found that an update process is running.
         *  It should download an update package using p2p downloader
         */

        auto installedVersions = m_clientUpdateTool->getInstalledClientVersions();
        bool isOk = m_serverUpdateTool->verifyUpdateManifest(updateInfo, installedVersions);

        if (updateInfo.isEmpty())
        {
            NX_INFO(this, "there is no active update info on the server");
            setTargetState(WidgetUpdateState::ready, {});
            return;
        }
        else if (!isOk)
        {
            NX_WARNING(this, "processInitialCheckState() - mediaservers have an active update "
                "process to version %1 with error \"%2\"", updateInfo.info.version, updateInfo.error);
        }
        else
        {
            NX_INFO(this, "processInitialCheckState() - mediaservers have an active update "
                "process to version %1", updateInfo.info.version);
        }

        // TODO: Client could have no update available for some reason. We should ignore it
        // if update process is already in 'installing' phase.
        if (m_updateInfo.preferOtherUpdate(updateInfo))
        {
            NX_INFO(this, "processInitialCheckState() - taking update info from mediaserver");
            setUpdateTarget(updateInfo, /*activeUpdate=*/true);
            m_clientUpdateTool->setUpdateTarget(updateInfo);
        }

        auto serversHaveDownloaded = m_stateTracker->peersInState(StatusCode::readyToInstall);
        auto serversAreDownloading = m_stateTracker->peersInState(StatusCode::downloading);
        auto serversWithError = m_stateTracker->peersInState(StatusCode::error);
        auto serversWithDownloadingError = m_stateTracker->peersWithDownloaderError();
        serversWithError.subtract(serversWithDownloadingError);
        auto peersAreInstalling = m_serverUpdateTool->getServersInstalling();
        auto serversHaveInstalled = m_stateTracker->peersCompleteInstall();

        bool hasClientUpdate = m_clientUpdateTool->shouldInstallThis(updateInfo);
        m_forceUiStateUpdate = true;

        if (!peersAreInstalling.empty() || updateInfo.alreadyInstalled)
        {
            NX_INFO(this,
                "processInitialCheckState() - servers %1 are installing an update",
                peersAreInstalling);

            if (hasClientUpdate)
                peersAreInstalling.insert(clientPeerId());
            setTargetState(WidgetUpdateState::installing,
                peersAreInstalling + serversHaveInstalled, false);
        }
        else if (!serversAreDownloading.empty() || !serversWithDownloadingError.empty())
        {
            auto targets = serversAreDownloading + serversWithDownloadingError;
            NX_INFO(this,
                "processInitialCheckState() - servers %1 are in downloading or error state",
                targets);

            auto uploaderState = m_serverUpdateTool->getUploaderState();
            if (uploaderState == ServerUpdateTool::OfflineUpdateState::push ||
                uploaderState == ServerUpdateTool::OfflineUpdateState::ready)
            {
                m_serverUpdateTool->startUpload(m_updateInfo);
            }

            setTargetState(WidgetUpdateState::downloading, targets);
            // TODO: We should check whether we have initiated an update. Maybe we should
            // not start manual uploads.
            if (!m_updateInfo.manualPackages.empty() && m_updateInfo.noServerWithInternet)
                m_serverUpdateTool->startManualDownloads(m_updateInfo);
        }
        else if (!serversHaveDownloaded.empty() || !serversWithError.empty())
        {
            NX_INFO(this,
                "processInitialCheckState() - servers %1 have already downloaded an update",
                serversHaveDownloaded);
            setTargetState(WidgetUpdateState::readyInstall, serversHaveDownloaded);
        }
        else if (!serversHaveInstalled.empty())
        {
            NX_INFO(this,
                "processInitialCheckState() - servers %1 have already installed an update",
                serversHaveInstalled);
            // We are here only if there are some offline servers and the rest of peers
            // have complete its update.
            setTargetState(WidgetUpdateState::readyInstall, {});
        }
        else
        {
            // We can reach here when we reconnect to the server with complete updates.
            NX_INFO(this, "processInitialCheckState() - no servers in "
                "downloading/installing/downloaded/installed state. "
                "Update process seems to be stalled or complete. Ignoring this internal state.");
            setTargetState(WidgetUpdateState::ready, {});
        }

        loadDataToUi();
    }
}

void MultiServerUpdatesWidget::processDownloadingState()
{
    m_stateTracker->processDownloadTaskSet();

    auto peersActive = m_stateTracker->peersActive();
    auto peersUnknown = m_stateTracker->peersWithUnknownStatus();
    auto peersFailed = m_stateTracker->peersFailed();
    auto peersComplete = m_stateTracker->peersComplete();
    auto peersIssued = m_stateTracker->peersIssued();

    // Starting uploads only when we get a proper data from /ec2/updateStatus.
    if (m_serverStatusCheck.valid()
        && m_serverStatusCheck.wait_for(kWaitForUpdateCheckFuture) == std::future_status::ready)
    {
        auto remoteStatus = m_serverStatusCheck.get();
        m_stateTracker->setUpdateStatus(remoteStatus);

        auto peersDownloading = m_stateTracker->peersInState(StatusCode::downloading);

        if (!peersDownloading.isEmpty()
            && peersFailed.isEmpty()
            && m_updateSourceMode == UpdateSourceType::file)
        {
            NX_VERBOSE(this, "processDownloadingState() - starting uploads");
            m_serverUpdateTool->startUpload(m_updateInfo);
        }
        else
        {
            NX_ERROR(this, "processDownloadingState() - no servers downloading or an error.");
        }
    }

    if (peersActive.size() + peersUnknown.size() > 0 && peersFailed.empty())
        return;

    // No peers are doing anything. So we consider current state transition is complete
    NX_INFO(this) << "processDownloadingState() - download has stopped";

    if (peersComplete.size() >= peersIssued.size())
    {
        // All servers have completed downloading process.
        auto complete = peersComplete;
        setTargetState(WidgetUpdateState::readyInstall, complete);
    }
    else
    {
        QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
        // 3. All other cases. Some servers have failed
        messageBox->setIcon(QnMessageBoxIcon::Critical);
        messageBox->setText(tr("Failed to download update packages to some components"));

        PeerStateTracker::ErrorReport report;
        m_stateTracker->getErrorReport(report);
        QString text = report.message;
        text += htmlParagraph(tr("If the problem persists, please contact Customer Support."));
        messageBox->setInformativeText(text);

        // TODO: Client can be here as well, but it would not be displayed.
        // Should we display it somehow?
        auto resourcesFailed = resourcePool()->getResourcesByIds(report.peers);
        injectResourceList(*messageBox, resourcesFailed);

        auto tryAgain = messageBox->addButton(tr("Try again"),
            QDialogButtonBox::AcceptRole);
        auto cancelUpdate = messageBox->addButton(tr("Cancel Update"),
            QDialogButtonBox::RejectRole);
        messageBox->setEscapeButton(cancelUpdate);
        messageBox->exec();

        auto clicked = messageBox->clickedButton();
        if (clicked == tryAgain)
        {
            auto serversToRetry = peersFailed;
            m_serverUpdateTool->requestRetryAction();
            m_clientUpdateTool->setUpdateTarget(m_updateInfo);
            setTargetState(WidgetUpdateState::downloading, serversToRetry);
            m_stateTracker->markStatusUnknown(serversToRetry);
        }
        else if (clicked == cancelUpdate)
        {
            setTargetState(WidgetUpdateState::cancelingDownload, {});
            m_serverUpdateTool->requestStopAction();
        }
    }

    processUploaderChanges();
}

void MultiServerUpdatesWidget::processReadyInstallState()
{
    m_stateTracker->processReadyInstallTaskSet();

    auto idle = m_stateTracker->peersInState(StatusCode::idle);
    auto all = m_stateTracker->allPeers();
    auto downloading = m_stateTracker->peersInState(StatusCode::downloading)
        + m_stateTracker->peersInState(StatusCode::preparing);
    downloading.subtract(m_stateTracker->offlineServers());

    if (!downloading.empty())
    {
        // We should go to downloading stage if we have merged
        // another system. This system will start update automatically, so we just need
        // to change UI state.
        NX_DEBUG(this, "processReadyInstallState() - detected servers %1 in downloading state",
            downloading);
        setTargetState(WidgetUpdateState::downloading, downloading, false);
    }
    else if (idle.size() == all.size() && m_serverUpdateTool->haveActiveUpdate())
    {
        setTargetState(WidgetUpdateState::ready, {});
    }
}

void MultiServerUpdatesWidget::processInstallingState()
{
    m_stateTracker->processInstallTaskSet();

    auto duration = m_serverUpdateTool->getInstallDuration();
    if (m_widgetState == WidgetUpdateState::installing
        && duration > kLongInstallWarningTimeout)
    {
        NX_VERBOSE(this) << "processRemoteInstalling() - detected stalled installation";
        setTargetState(WidgetUpdateState::installingStalled, m_stateTracker->peersIssued());
    }

    auto peersInstalling = m_stateTracker->peersInstalling();
    auto peersComplete = m_stateTracker->peersComplete();
    auto readyToInstall = m_stateTracker->peersInState(StatusCode::readyToInstall);
    auto peersFailed = m_stateTracker->peersFailed();

    // No peers are doing anything right now. We should check if installation is complete.
    if (peersInstalling.empty())
    {
        auto peersIssued = m_stateTracker->peersIssued();
        NX_INFO(this,
            "processRemoteInstalling() - no peers are installing. Moving to 'finishInstall'");
        setTargetState(WidgetUpdateState::finishingInstall, peersIssued);
    }

    if (!readyToInstall.empty())
    {
        // Client could connect to a system with update in 'installing' state. So client could
        // complete downloading its files only just now. So we need to ask it to start
        // installation.
        auto clientId = clientPeerId();
        if (readyToInstall.contains(clientId))
            m_clientUpdateTool->installUpdateAsync();
    }
}

void MultiServerUpdatesWidget::completeClientInstallation(bool clientUpdated)
{
    auto updatedProtocol = m_stateTracker->serversWithChangedProtocol();
    bool clientInstallerRequired = false;
    if (clientUpdated && !clientInstallerRequired)
    {
        QString authString = m_serverUpdateTool->getServerAuthString();
        NX_INFO(this, "completeInstallation() - restarting the client");
        if (!m_clientUpdateTool->restartClient(authString))
        {
            NX_ERROR(this, "completeInstallation(%1) - failed to run restart command",
                clientUpdated);
            QnConnectionDiagnosticsHelper::failedRestartClientMessage(this);
        }
        else
        {
            applauncher::api::scheduleProcessKill(QCoreApplication::applicationPid(), kProcessTerminateTimeoutMs);
            qApp->exit(0);
            return;
        }
    }

    qnClientMessageProcessor->setHoldConnection(false);

    if (!updatedProtocol.empty())
    {
        NX_INFO(this, "completeInstallation() - servers %1 have new protocol. Forcing reconnect",
            updatedProtocol);
        menu()->trigger(action::DisconnectAction, {Qn::ForceRole, true});
    }

    setUpdateSourceMode(UpdateSourceType::internet);
    setTargetState(WidgetUpdateState::initial);
}

bool MultiServerUpdatesWidget::processRemoteChanges()
{
    // We gather here updated server status from updateTool
    // and change WidgetUpdateState state accordingly.

    // TODO: It could be moved to UpdateTool
    ServerUpdateTool::RemoteStatus remoteStatus;
    if (m_serverUpdateTool->getServersStatusChanges(remoteStatus))
    {
        if (m_stateTracker->setUpdateStatus(remoteStatus) > 0)
            m_updateRemoteStateChanged = true;
    }

    m_stateTracker->processUnknownStates();

    m_clientUpdateTool->checkInternalState();

    if (m_widgetState == WidgetUpdateState::initial)
        processInitialState();

    if (m_widgetState == WidgetUpdateState::initialCheck)
        processInitialCheckState();

    if (m_widgetState == WidgetUpdateState::downloading)
    {
        processDownloadingState();
    }
    else if (m_widgetState == WidgetUpdateState::installing
        || m_widgetState == WidgetUpdateState::installingStalled)
    {
        processInstallingState();
    }
    else if (m_widgetState == WidgetUpdateState::readyInstall)
    {
        processReadyInstallState();
    }

    // TODO: m_widgetState changes are already tracked.
    m_updateRemoteStateChanged = true;

    return true;
}

bool MultiServerUpdatesWidget::processUploaderChanges(bool force)
{
    if (!m_serverUpdateTool->hasOfflineUpdateChanges() && !force)
        return false;
    auto state = m_serverUpdateTool->getUploaderState();

    if (state == ServerUpdateTool::OfflineUpdateState::error)
    {
        // TODO: We should deal with errors here. Should we?
        NX_INFO(this, "processUploaderChanges failed to upload all packages", force);
        setTargetState(WidgetUpdateState::ready, {});
    }
    return true;
}

void MultiServerUpdatesWidget::setTargetState(
    WidgetUpdateState state, const QSet<QnUuid>& targets, bool runCommands)
{
    bool clearTaskSet = true;
    if (m_widgetState != state)
    {
        NX_VERBOSE(this, "setTargetState() from %1 to %2", toString(m_widgetState),
            toString(state));
        bool stopProcess = false;
        switch (state)
        {
            case WidgetUpdateState::initial:
                stopProcess = true;
                m_stateTracker->clearState();
                break;
            case WidgetUpdateState::ready:
                stopProcess = true;
                break;
            case WidgetUpdateState::downloading:
                if (m_rightPanelDownloadProgress.isNull() && ini().systemUpdateProgressInformers)
                {
                    auto manager = context()->instance<WorkbenchProgressManager>();
                    // TODO: We should show 'Pushing updates...'
                    m_rightPanelDownloadProgress = manager->add(tr("Downloading updates..."));
                }
                break;
            case WidgetUpdateState::readyInstall:
                stopProcess = true;
                break;
            case WidgetUpdateState::installingStalled:
                break;
            case WidgetUpdateState::startingInstall:
                NX_ASSERT(!targets.empty());
                if (runCommands && !targets.empty())
                {
                    QSet<QnUuid> servers = targets;
                    servers.remove(clientPeerId());
                    m_serverUpdateTool->requestInstallAction(servers);
                }
                // The rest will be done in a handler for WidgetUpdateState::installing.
                break;
            case WidgetUpdateState::installing:
                m_stateTracker->setPeersInstalling(targets, true);
                if (runCommands && !targets.empty())
                {
                    QSet<QnUuid> servers = targets;
                    servers.remove(clientPeerId());
                    if (!servers.empty())
                        qnClientMessageProcessor->setHoldConnection(true);
                }

                m_installCheckTimer = std::make_unique<QTimer>(this);
                connect(m_installCheckTimer.get(), &QTimer::timeout,
                    this, &MultiServerUpdatesWidget::atCheckInstallState);
                m_installCheckTimer->setSingleShot(true);
                m_installCheckTimer->start(kDelayForCheckingInstallStatus);

                if (m_clientUpdateTool->hasUpdate())
                    m_clientUpdateTool->installUpdateAsync();
                break;

            case WidgetUpdateState::finishingInstall:
                clearTaskSet = false;
                m_serverUpdateTool->requestFinishUpdate(/*skipActivePeers=*/true);
                qnClientMessageProcessor->setHoldConnection(false);
                break;
            case WidgetUpdateState::complete:
                stopProcess = true;
                break;
            default:
                break;
        }

        if (stopProcess)
        {
            closePanelNotifications();
        }
    }
    // TODO: We could try to move to 'far' states, with no direct transition.
    // Should invent something for such case.
    m_widgetState = state;
    m_stateTracker->setTask(targets, clearTaskSet);
    m_forceUiStateUpdate = true;
}

void MultiServerUpdatesWidget::closePanelNotifications()
{
    if (m_rightPanelDownloadProgress.isNull())
        return;
    auto manager = context()->instance<WorkbenchProgressManager>();
    manager->remove(m_rightPanelDownloadProgress);
    m_rightPanelDownloadProgress = QnUuid();
}

void MultiServerUpdatesWidget::syncVersionReport(const VersionReport& report)
{
    const auto setHighlightMode =
        [](QLabel* label, VersionReport::HighlightMode mode)
        {
            if (mode == VersionReport::HighlightMode::bright)
            {
                QFont versionLabelFont;
                versionLabelFont.setPixelSize(kVersionLabelFontSizePixels);
                versionLabelFont.setWeight(kVersionLabelFontWeight);
                label->setFont(versionLabelFont);
                label->setProperty(style::Properties::kDontPolishFontProperty, true);
            }
            else if (mode == VersionReport::HighlightMode::red)
                setWarningStyle(label);
            else
                resetStyle(label);
        };

    ui->errorLabel->setText(report.statusMessages.join("<br>"));
    if (report.version.isEmpty())
        ui->targetVersionLabel->setText(kNoVersionNumberText);
    else
        ui->targetVersionLabel->setText(report.version);

    setHighlightMode(ui->targetVersionLabel, report.versionHighlight);
    setHighlightMode(ui->errorLabel, report.statusHighlight);
}

bool MultiServerUpdatesWidget::isChecking() const
{
    if (hasActiveUpdate())
        return false;
    return m_updateCheck.valid()
        || m_serverUpdateCheck.valid()
        || m_widgetState == WidgetUpdateState::initial;
}

bool MultiServerUpdatesWidget::hasActiveUpdate() const
{
    return m_widgetState == WidgetUpdateState::downloading
        || m_widgetState == WidgetUpdateState::readyInstall
        || m_widgetState == WidgetUpdateState::installing;
}

bool MultiServerUpdatesWidget::hasLatestVersion() const
{
    const bool hasEqualUpdateInfo = m_stateTracker->lowestInstalledVersion() >= m_updateInfo.getVersion();
    bool hasLatestVersion = false;
    if (m_updateInfo.isEmpty() || m_updateInfo.error == nx::update::InformationError::noNewVersion)
        hasLatestVersion = true;
    else if (hasEqualUpdateInfo || m_updateInfo.alreadyInstalled)
        hasLatestVersion = true;

    if (m_updateInfo.error != nx::update::InformationError::noNewVersion
        && m_updateInfo.error != nx::update::InformationError::noError)
    {
        hasLatestVersion = false;
    }

    if (m_widgetState != WidgetUpdateState::ready
        && m_widgetState != WidgetUpdateState::initial
        && hasLatestVersion)
    {
        hasLatestVersion = false;
    }

    if (isChecking())
        hasLatestVersion = false;

    return hasLatestVersion;
}

void MultiServerUpdatesWidget::syncUpdateCheckToUi()
{
    const bool isChecking = this->isChecking();

    bool latestVersion = hasLatestVersion();

    switch(m_widgetState)
    {
        case WidgetUpdateState::startingDownload:
        case WidgetUpdateState::startingInstall:
        case WidgetUpdateState::installing:
            ui->cancelProgressAction->setEnabled(false);
            break;
        default:
            ui->cancelProgressAction->setEnabled(true);
    }

    if (m_widgetState == WidgetUpdateState::installingStalled)
        ui->cancelProgressAction->setText(tr("Finish Update"));
    else
        ui->cancelProgressAction->setText(tr("Cancel"));

    ui->selectUpdateTypeButton->setEnabled(!isChecking);

    if (isChecking)
    {
        ui->versionStackedWidget->setCurrentWidget(ui->checkingPage);
        ui->updateCheckMode->setVisible(false);
        ui->releaseDescriptionLabel->setText(QString());
        ui->errorLabel->setText(QString());
    }
    else
    {
        const bool validInstall = m_updateInfo.isValidToInstall()
            && !m_updateInfo.peersWithUpdate.empty();
        ui->downloadButton->setVisible(
            validInstall || m_widgetState == WidgetUpdateState::readyInstall);

        if (latestVersion)
        {
            if (m_updateInfo.sourceType == UpdateSourceType::internet)
                ui->latestVersionBannerLabel->setText(tr("The latest version is already installed"));
            else
                ui->latestVersionBannerLabel->setText(tr("This version is already installed"));
            ui->versionStackedWidget->setCurrentWidget(ui->latestVersionPage);
        }
        else
        {
            if (validInstall)
            {
                if (m_widgetState == WidgetUpdateState::readyInstall)
                {
                    ui->downloadButton->setText(tr("Install update"));
                }
                else
                {
                    if (m_updateSourceMode == UpdateSourceType::file)
                    {
                        ui->downloadButton->setText(tr("Upload"));
                        ui->downloadAndInstall->setText(tr("Upload && Install"));
                    }
                    else // LatestVersion or SpecificBuild)
                    {
                        ui->downloadButton->setText(tr("Download"));
                        ui->downloadAndInstall->setText(tr("Download && Install"));
                    }
                }

                ui->releaseDescriptionLabel->setText(m_updateInfo.info.description);
                ui->releaseDescriptionLabel->setVisible(!m_updateInfo.info.description.isEmpty());
            }

            ui->versionStackedWidget->setCurrentWidget(ui->versionPage);
        }

        bool browseUpdateVisible = false;
        if (/*!m_haveValidUpdate && */m_widgetState == WidgetUpdateState::ready)
        {
            if (m_updateInfo.isValidToInstall())
            {
                if (m_updateSourceMode == UpdateSourceType::file)
                {
                    browseUpdateVisible = true;
                    ui->browseUpdate->setText(tr("Browse for Another File..."));
                }
                else if (m_updateSourceMode == UpdateSourceType::internetSpecific && latestVersion)
                {
                    browseUpdateVisible = true;
                    ui->browseUpdate->setText(tr("Select Another Build"));
                }
            }
            else if (m_updateSourceMode == UpdateSourceType::internet)
            {
                browseUpdateVisible = true;
                ui->browseUpdate->setText(tr("Update to Specific Build"));
            }
            else if (m_updateSourceMode == UpdateSourceType::internetSpecific)
            {
                browseUpdateVisible = true;
                ui->browseUpdate->setText(tr("Select Another Build"));
            }
            else if (m_updateSourceMode == UpdateSourceType::file)
            {
                browseUpdateVisible = true;
                ui->browseUpdate->setText(tr("Browse for Another File..."));
            }
        }
        ui->browseUpdate->setVisible(browseUpdateVisible);
        ui->latestVersionIconLabel->setVisible(latestVersion);
        ui->updateCheckMode->setVisible(m_updateSourceMode == UpdateSourceType::internet);
        m_updatesModel->setUpdateTarget(m_updateInfo.getVersion());
    }

    ui->selectUpdateTypeButton->setText(toString(m_updateSourceMode));

    const bool showButton = !isChecking && !latestVersion
        && m_updateSourceMode != UpdateSourceType::file
        && (m_widgetState == WidgetUpdateState::ready
            || m_widgetState != WidgetUpdateState::initial)
        && (m_updateInfo.error == nx::update::InformationError::networkError
            // If one wants to download a file in another place.
            || m_updateInfo.error == nx::update::InformationError::noError)
        && m_widgetState != WidgetUpdateState::readyInstall
        // httpError corresponds to 'Build not found'
        && m_updateInfo.error != nx::update::InformationError::httpError;

    ui->manualDownloadButton->setVisible(showButton || ini().alwaysShowGetUpdateFileButton);

    syncVersionReport(m_updateReport);
}

bool MultiServerUpdatesWidget::stateHasProgress(WidgetUpdateState state)
{
    switch (state)
    {
        case WidgetUpdateState::initial:
        case WidgetUpdateState::initialCheck:
        case WidgetUpdateState::ready:
        case WidgetUpdateState::readyInstall:
        case WidgetUpdateState::cancelingReadyInstall:
        case WidgetUpdateState::complete:
            return false;
        case WidgetUpdateState::startingDownload:
        case WidgetUpdateState::startingInstall:
        case WidgetUpdateState::downloading:
        case WidgetUpdateState::cancelingDownload:
        case WidgetUpdateState::installing:
        case WidgetUpdateState::installingStalled:
        case WidgetUpdateState::finishingInstall:
            return true;
    }
    return false;
}

void MultiServerUpdatesWidget::syncProgress()
{
    ServerUpdateTool::ProgressInfo info = calculateActionProgress();
    ui->actionProgess->setValue(info.current);
    ui->actionProgess->setMaximum(info.max);

    QString caption;
    if (info.uploading)
        caption = tr("Uploading updates...");
    else if (info.downloadingServers)
        caption = tr("Downloading updates...");
    else if (info.downloadingClient)
        caption = tr("Downloading client package...");
    else if (info.installingServers)
        caption = tr("Installing updates...");
    else if (info.installingClient)
        caption = tr("Installing client updates...");

    ui->actionProgess->setTextVisible(!caption.isEmpty());
    if (!caption.isEmpty())
        ui->actionProgess->setFormat(caption + "\t");

    if (!m_rightPanelDownloadProgress.isNull())
    {
        auto manager = context()->instance<WorkbenchProgressManager>();
        qreal progress = info.max > 0 ? info.current/qreal(info.max) : -1.0;
        manager->setProgress(m_rightPanelDownloadProgress, progress);
    }

    syncDebugInfoToUi();
}

void MultiServerUpdatesWidget::syncRemoteUpdateStateToUi()
{
    // TODO: We should fill in ControlPanelState structure here,
    // instead of affecting UI state directly

    // Title to be shown for this UI state.
    QString updateTitle;

    bool storageSettingsVisible = false;
    // We have cut storageSettings from 4.0, but I do not want to remove this from the code yet.
    // This will suppress the warning.
    Q_UNUSED(storageSettingsVisible);

    switch (m_widgetState)
    {
        case WidgetUpdateState::initial:
            storageSettingsVisible = true;
            break;
        case WidgetUpdateState::ready:
            if (m_updateInfo.isValidToInstall())
                storageSettingsVisible = true;
            break;
        case WidgetUpdateState::startingDownload:
            updateTitle = tr("Starting update to ...");
            break;
        case WidgetUpdateState::downloading:
            updateTitle = tr("Updating to ...");
            break;
        case WidgetUpdateState::readyInstall:
            updateTitle = tr("Ready to update to");
            break;
        case WidgetUpdateState::installing:
            updateTitle = tr("Updating to ...");
            break;
        case WidgetUpdateState::complete:
            updateTitle = tr("System updated to");
            break;
        default:
            break;
    }

    // Making this button invisible for now, until a proper mediaserver selection is impelemented.
    //ui->advancedUpdateSettings->setVisible(storageSettingsVisible);
    //ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn,
    //    !storageSettingsVisible || !m_showStorageSettings);
    ui->advancedUpdateSettings->setVisible(false);
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn, true);

    ui->cancelUpdateButton->setVisible(m_widgetState == WidgetUpdateState::readyInstall);

    // Updating title. That is the upper part of the window
    QWidget* selectedTitle = ui->selectUpdateTypePage;
    if (!updateTitle.isEmpty())
    {
        selectedTitle = ui->updatingPage;
        ui->updatingTitleLabel->setText(updateTitle);
    }

    if (selectedTitle && selectedTitle != ui->titleStackedWidget->currentWidget())
    {
        NX_VERBOSE(this) << "syncRemoteUpdateState() - updating titleStackedWidget";
        ui->titleStackedWidget->setCurrentWidget(selectedTitle);
    }

    if (isChecking())
        ui->updateStackedWidget->setCurrentWidget(ui->emptyPage);
    else if (stateHasProgress(m_widgetState))
        ui->updateStackedWidget->setCurrentWidget(ui->updateProgressPage);
    else if (m_widgetState == WidgetUpdateState::complete)
        ui->updateStackedWidget->setCurrentWidget(ui->emptyPage);
    else
        ui->updateStackedWidget->setCurrentWidget(ui->updateControlsPage);

    // TODO: It should be moved to loadDataToUi
    if (stateHasProgress(m_widgetState))
        syncProgress();

    bool hasVerificationErrors = m_stateTracker->hasVerificationErrors();
    bool hasStatusErrors = m_stateTracker->hasStatusErrors();
    bool hasSpaceIssues = !checkSpaceRequirements(m_updateInfo)
        && m_widgetState == WidgetUpdateState::ready;
    bool nothingToInstall = m_updateInfo.peersWithUpdate.empty();

    QStringList errorTooltips;
    if (m_widgetState == WidgetUpdateState::readyInstall
        || m_widgetState == WidgetUpdateState::ready)
    {
        auto peersIssued = m_stateTracker->peersIssued();
        auto peersActive = m_stateTracker->peersActive();
        //auto readyAndOnline = m_stateTracker->onlineAndInState(LocalStatusCode::readyToInstall);
        //auto readyAndOffline = m_stateTracker->offlineAndInState(LocalStatusCode::readyToInstall);
        bool hasInstallIssues = (peersIssued.empty() || (peersActive.size() < peersIssued.size())
            || !m_stateTracker->peersFailed().empty())
            && m_widgetState == WidgetUpdateState::readyInstall;

        if (hasInstallIssues || hasStatusErrors || hasVerificationErrors)
        {
            if (hasVerificationErrors)
            {
                errorTooltips << tr("Some servers have no update packages available.");
            }
            else if (hasStatusErrors)
            {
                errorTooltips << tr("Some servers have encountered an internal error.");
                errorTooltips << tr("Please contact Customer Support.");
            }
            else if (hasInstallIssues)
            {
                errorTooltips << tr("Some servers have gone offline. "
                                    "Please wait until they become online to continue.");
            }
        }
    }

    if (hasSpaceIssues)
    {
        setWarningStyle(ui->spaceErrorLabel);
        ui->spaceErrorLabel->setText("Client does not have enough space to download update packages.");
        ui->spaceErrorLabel->show();
    }
    else
    {
        ui->spaceErrorLabel->hide();
    }

    // TODO: We should move all code about ui->downloadButton to a single place.
    if (errorTooltips.isEmpty() && !hasSpaceIssues && !nothingToInstall
        && m_widgetState != WidgetUpdateState::cancelingReadyInstall)
    {
        ui->downloadButton->setEnabled(true);
        ui->downloadButton->setToolTip("");
    }
    else
    {
        ui->downloadButton->setEnabled(false);
        ui->downloadButton->setToolTip(errorTooltips.join("\n"));
    }

    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn, !m_showStorageSettings);
    QString icon = m_showStorageSettings
        ? QString("text_buttons/expand.png")
        : QString("text_buttons/collapse.png");
    ui->advancedUpdateSettings->setIcon(qnSkin->icon(icon));
    if (m_showStorageSettings)
        ui->tableView->setEditTriggers(QAbstractItemView::AllEditTriggers);
    else
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MultiServerUpdatesWidget::loadDataToUi()
{
    NX_ASSERT(m_serverUpdateTool);

    // Dealing with 'status' column inside servers table.
    m_statusColumnMode = calculateStatusColumnVisibility();
    if (m_statusColumnMode.changed())
    {
        m_statusItemDelegate->setStatusMode(m_statusColumnMode);
        ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::ProgressColumn,
            m_statusColumnMode == ServerStatusItemDelegate::StatusMode::hidden);
        m_updatesModel->forceUpdateColumn(ServerUpdatesModel::Columns::ProgressColumn);
        // This will force delegate update all its widgets.
        ui->tableView->update();
        m_statusColumnMode.acceptChanges();
    }

    // Update UI state to match modes: {SpecificBuild;LatestVersion;LocalFile}
    if (m_forceUiStateUpdate || m_updateRemoteStateChanged)
    {
        // This one depends both on local and remote information.
        syncUpdateCheckToUi();
    }

    if (m_forceUiStateUpdate || m_updateRemoteStateChanged)
        syncRemoteUpdateStateToUi();

    bool endOfTheWeek = QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek;
    ui->dayWarningBanner->setVisible(endOfTheWeek);
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StatusMessageColumn, !m_showDebugData);

    syncDebugInfoToUi();
    syncVersionInfoVisibility();

    if (auto layout = ui->versionStackedWidget->currentWidget()->layout(); NX_ASSERT(layout))
        layout->activate();

    ui->controlsVerticalLayout->activate();
    m_forceUiStateUpdate = false;
    m_updateRemoteStateChanged = false;
}

void MultiServerUpdatesWidget::syncVersionInfoVisibility()
{
    const bool hasError = !ui->errorLabel->text().isEmpty();
    ui->errorLabel->setVisible(hasError);

    const bool hasReleaseDescription = !ui->releaseDescriptionLabel->text().isEmpty();
    ui->releaseDescriptionHolder->setVisible(hasReleaseDescription  && !hasError);

    const bool hasReleaseNotes = !m_updateInfo.info.releaseNotesUrl.isEmpty();
    ui->releaseNotesUrl->setVisible(hasReleaseNotes && !hasError);
}

void MultiServerUpdatesWidget::syncDebugInfoToUi()
{
    if (m_showDebugData)
    {
        QString combinePath = qnSettings->updateCombineUrl();
        if (combinePath.isEmpty())
            combinePath = QnAppInfo::updateGeneratorUrl();

        QStringList debugState = {
            QString("UpdateUrl=<a href=\"%1\">%1</a>").arg(qnSettings->updateFeedUrl()),
            QString("UpdateFeedUrl=<a href=\"%1\">%1</a>").arg(combinePath),
            QString("Widget=%1").arg(toString(m_widgetState)),
            QString("Widget source=%1, Update source=%2").arg(toString(m_updateSourceMode), toString(m_updateInfo.sourceType)),
            QString("UploadTool=%1").arg(toString(m_serverUpdateTool->getUploaderState())),
            QString("ClientTool=%1").arg(ClientUpdateTool::toString(m_clientUpdateTool->getState())),
            QString("validUpdate=%1").arg(m_updateInfo.isValidToInstall()),
            QString("targetVersion=%1").arg(m_updateInfo.info.version),
            QString("checkUpdate=%1").arg(m_updateCheck.valid()),
            QString("checkServerUpdate=%1").arg(m_serverUpdateCheck.valid()),
            QString("<a href=\"%1\">/ec2/updateStatus</a>").arg(m_serverUpdateTool->getUpdateStateUrl()),
            QString("<a href=\"%1\">/ec2/updateInformation</a>").arg(m_serverUpdateTool->getUpdateInformationUrl()),
            QString("<a href=\"%1\">/ec2/updateInformation?version=installed</a>").arg(m_serverUpdateTool->getInstalledUpdateInfomationUrl()),
        };

        debugState << QString("lowestVersion=%1").arg(m_stateTracker->lowestInstalledVersion().toString());

        QStringList report;
        for (auto server: m_serverUpdateTool->getServersInstalling())
            report << server.toString();
        if (!report.empty())
            debugState << QString("installing=%1").arg(report.join(","));

        report.clear();
        for (auto peer: m_stateTracker->peersIssued())
            report << peer.toString();
        if (!report.empty())
            debugState << QString("issued=%1").arg(report.join(","));

        report.clear();
        for (auto peer: m_stateTracker->peersFailed())
            report << peer.toString();
        if (!report.empty())
            debugState << QString("failed=%1").arg(report.join(","));

        if (m_updateInfo.error != nx::update::InformationError::noError)
        {
            QString internalError = nx::update::toString(m_updateInfo.error);
            debugState << QString("updateInfoError=%1").arg(internalError);
        }

        if (stateHasProgress(m_widgetState))
        {
            ServerUpdateTool::ProgressInfo info = calculateActionProgress();
            debugState << lm("progress=%1 of %2, active=%3, done=%4").args(info.current, info.max, info.active, info.done);
        }

        if (m_widgetState == WidgetUpdateState::installing
            || m_widgetState == WidgetUpdateState::installingStalled)
        {
            auto installDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                m_serverUpdateTool->getInstallDuration());
            debugState << QString("duration=%1").arg(installDuration.count());
        }

        if (m_updateInfo.isValidToInstall() && m_updateInfo.noServerWithInternet)
            debugState << QString("noServerWithInternet");

        QString debugText = debugState.join("<br>");
        if (debugText != ui->debugStateLabel->text())
            ui->debugStateLabel->setText(debugText);
    }

    ui->debugStateLabel->setVisible(m_showDebugData);
}

void MultiServerUpdatesWidget::applyChanges()
{
    // TODO: Update logic for auto check
    qnGlobalSettings->setUpdateNotificationsEnabled(m_autoCheckMenu);
    qnGlobalSettings->synchronizeNow();
}

void MultiServerUpdatesWidget::discardChanges()
{
    // TODO: We should ask user only if we were pushing updates.
    // For the reset cases we can not tell which client instance
    // was controlling state of updates.
    if (m_serverUpdateTool->getUploaderState() == ServerUpdateTool::OfflineUpdateState::push)
    {
        QnMessageBox dialog(QnMessageBoxIcon::Information,
            tr("System update in process"), QString(),
            QDialogButtonBox::NoButton, QDialogButtonBox::NoButton,
            this);

        const auto cancelUpdateButton = dialog.addButton(
            tr("Cancel Update"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
        dialog.addButton(
            tr("Continue in Background"), QDialogButtonBox::RejectRole);

        dialog.exec();
        if (dialog.clickedButton() == cancelUpdateButton)
            atCancelCurrentAction();
    }
}

bool MultiServerUpdatesWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    //return qnGlobalSettings->isUpdateNotificationsEnabled() != ui->autoCheckUpdates->isChecked();
    return false;
}

bool MultiServerUpdatesWidget::canDiscardChanges() const
{
    // TODO: #GDM now this prevents other tabs from discarding their changes
    return true;
}

void MultiServerUpdatesWidget::autoCheckForUpdates()
{
    if (m_updateSourceMode != UpdateSourceType::internet)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAutoUpdateCheck < kAutoCheckIntervalMs)
        return;

    checkForInternetUpdates();
}

ServerStatusItemDelegate::StatusMode MultiServerUpdatesWidget::calculateStatusColumnVisibility() const
{
    using StatusMode = ServerStatusItemDelegate::StatusMode;
    StatusMode statusMode = StatusMode::remoteStatus;

    if (m_widgetState == WidgetUpdateState::initial
        || m_widgetState == WidgetUpdateState::ready
        || m_widgetState == WidgetUpdateState::cancelingReadyInstall)
    {
        statusMode = StatusMode::hidden;
    }

    if (m_stateTracker->hasVerificationErrors())
        statusMode = StatusMode::reportErrors;
    else if (m_stateTracker->hasStatusErrors())
        statusMode = StatusMode::remoteStatus;
    else if (m_updateInfo.alreadyInstalled)
        statusMode = StatusMode::hidden;

    return statusMode;
}

void MultiServerUpdatesWidget::atModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& /*unused*/)
{
    // This forces custom editor to appear for every item in the table.
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        QModelIndex index = m_sortedModel->index(row, ServerUpdatesModel::ProgressColumn);
        ui->tableView->openPersistentEditor(index);
    }
}

QString MultiServerUpdatesWidget::toString(WidgetUpdateState state)
{
    // These strings are internal and are not intended to be visible to regular user.
    switch (state)
    {
        case WidgetUpdateState::initial:
            return "Initial";
        case WidgetUpdateState::initialCheck:
            return "CheckServers";
        case WidgetUpdateState::ready:
            return "Ready";
        case WidgetUpdateState::startingDownload:
            return "StartingDownload";
        case WidgetUpdateState::downloading:
            return "Downloading";
        case WidgetUpdateState::cancelingDownload:
            return "CancelingDownload";
        case WidgetUpdateState::readyInstall:
            return "ReadyInstall";
        case WidgetUpdateState::cancelingReadyInstall:
            return "CancelingReadyInstall";
        case WidgetUpdateState::startingInstall:
            return "StartingInstall";
        case WidgetUpdateState::installing:
            return "Installing";
        case WidgetUpdateState::installingStalled:
            return "InstallationStalled";
        case WidgetUpdateState::finishingInstall:
            return "FinishingInstall";
        case WidgetUpdateState::complete:
            return "Complete";
        default:
            return "Unknown";
    }
}

QString MultiServerUpdatesWidget::toString(ServerUpdateTool::OfflineUpdateState state)
{
    // These strings are internal and are not intended to be visible to regular user.
    switch (state)
    {
        case ServerUpdateTool::OfflineUpdateState::initial:
            return "initial";
        case ServerUpdateTool::OfflineUpdateState::unpack:
            return "unpack";
        case ServerUpdateTool::OfflineUpdateState::ready:
            return "ready";
        case ServerUpdateTool::OfflineUpdateState::preparing:
            return "preparing";
        case ServerUpdateTool::OfflineUpdateState::push:
            return "push";
        case ServerUpdateTool::OfflineUpdateState::done:
            return "done";
        case ServerUpdateTool::OfflineUpdateState::error:
            return "error";
    }
    return "Unknown update source mode";
}

QString MultiServerUpdatesWidget::toString(nx::update::UpdateSourceType mode)
{
    switch (mode)
    {
        case nx::update::UpdateSourceType::internet:
            return tr("Latest Available Update");
        case nx::update::UpdateSourceType::internetSpecific:
            return tr("Specific Build");
        case nx::update::UpdateSourceType::file:
            return tr("Browse for Update File");
        case nx::update::UpdateSourceType::mediaservers:
            // This string should not appear at UI.
            return tr("Update from mediaservers");
    }
    return "Unknown update source mode";
}

bool MultiServerUpdatesWidget::VersionReport::operator==(const VersionReport& another) const
{
    return hasLatestVersion == another.hasLatestVersion
        && checking == another.checking
        && version == another.version
        && statusMessages == another.statusMessages
        && versionMode == another.versionMode
        && versionHighlight == another.versionHighlight
        && statusHighlight == another.statusHighlight;
}

bool MultiServerUpdatesWidget::ControlPanelState::operator==(
    const ControlPanelState& another) const
{
    return actionEnabled == another.actionEnabled
        && actionCaption == another.actionCaption
        && actionTooltips == another.actionTooltips
        && cancelEnabled == another.cancelEnabled
        && showManualDownload == another.showManualDownload
        && panelMode == another.panelMode
        && progressCaption == another.progressCaption
        && cancelProgressCaption == another.cancelProgressCaption
        && cancelProgressEnabled == another.cancelProgressEnabled
        && progressMinimum == another.progressMinimum
        && progressMaximum == another.progressMaximum
        && progress == another.progress;
}

} // namespace nx::vms::client::desktop
