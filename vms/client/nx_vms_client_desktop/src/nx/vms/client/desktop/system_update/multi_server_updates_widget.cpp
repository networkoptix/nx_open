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

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/eula_dialog.h>
#include <nx/vms/client/desktop/workbench/extensions/workbench_progress_manager.h>
#include <nx/network/app_info.h>

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
const auto kExpectedInstallPeriodSec = 4.0;

// Height limit for servers list in dialog box with update report
static constexpr int kSectionHeight = 150;
static constexpr int kSectionMinHeight = 30;

constexpr auto kLatestVersionBannerLabelFontSizePixels = 22;
constexpr auto kLatestVersionBannerLabelFontWeight = QFont::Light;

const auto kWaitForUpdateCheck = std::chrono::milliseconds(1);

const int kLinkCopiedMessageTimeoutMs = 2000;

// Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
const quint32 kProcessTerminateTimeoutMs = 15000;

/* N-dash 5 times: */
const QString kNoVersionNumberText = QString::fromWCharArray(L"\x2013\x2013\x2013\x2013\x2013");

constexpr int kVersionSelectionBlockHeight = 20;
constexpr int kVersionInformationBlockMinHeight = 48;
constexpr int kPreloaderHeight = 32;

// Adds resource list to message box
void injectResourceList(QnMessageBox& messageBox, const QnResourceList& resources)
{
    if (resources.empty())
        return;

    auto resourceList = new QTableView();
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
    // Adding this to QnMessageBox::Layout::Main looks ugly.
    // Adding to Layout::Content (default value) looks much better.
    messageBox.addCustomWidget(resourceList);
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
    m_clientUpdateTool.reset(new ClientUpdateTool(this));

    m_updateCheck = watcher->takeUpdateCheck();

    m_updatesModel = m_serverUpdateTool->getModel();
    m_stateTracker = m_serverUpdateTool->getStateTracker();
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
    ui->tableView->setItemDelegateForColumn(ServerUpdatesModel::ProgressColumn, m_statusItemDelegate.get());

    connect(m_sortedModel.get(), &SortedPeerUpdatesModel::dataChanged,
        this, &MultiServerUpdatesWidget::atModelDataChanged);

    // the column does not matter because the model uses column-independent sorting
    m_sortedModel->sort(0);

    if (auto horizontalHeader = ui->tableView->horizontalHeader())
    {
        horizontalHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::ProgressColumn, QHeaderView::Stretch);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::StatusMessageColumn,
            QHeaderView::Stretch);
        horizontalHeader->setSectionsClickable(false);
    }

    connect(m_serverUpdateTool.get(), &ServerUpdateTool::packageDownloaded,
        this, &MultiServerUpdatesWidget::atServerPackageDownloaded);

    connect(m_serverUpdateTool.get(), &ServerUpdateTool::packageDownloadFailed,
        this, &MultiServerUpdatesWidget::atServerPackageDownloadFailed);

    connect(m_clientUpdateTool.get(), &ClientUpdateTool::updateStateChanged,
        m_stateTracker.get(), &PeerStateTracker::atClientupdateStateChanged);

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
    // This button is hidden for now. We will implement it in future.
    ui->downloadAndInstall->hide();

    connect(ui->releaseNotesLabel, &QLabel::linkActivated, this,
        [this]()
        {
            if (m_haveValidUpdate && !m_updateInfo.info.releaseNotesUrl.isEmpty())
                QDesktopServices::openUrl(m_updateInfo.info.releaseNotesUrl);
        });

    connect(qnGlobalSettings, &QnGlobalSettings::updateNotificationsChanged, this,
        [this]()
        {
            setAutoUpdateCheckMode(qnGlobalSettings->isUpdateNotificationsEnabled());
        });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
        this, &MultiServerUpdatesWidget::atCloudSettingsChanged);

    connect(qnGlobalSettings, &QnGlobalSettings::localSystemIdChanged, this,
        [this]()
        {
            NX_DEBUG(this, "detected change in localSystemId. Need to refresh server list");

            if (m_stateTracker)
            {
                if (m_stateTracker->setResourceFeed(resourcePool()))
                {
                    // We will be here when we connected to another system.
                    // We should run update check again. This should fix VMS-13037.
                    if (m_widgetState == WidgetUpdateState::initial && !m_updateCheck.valid())
                    {
                        QString updateUrl = qnSettings->updateFeedUrl();
                        m_updateCheck = m_serverUpdateTool->checkLatestUpdate(updateUrl);
                    }
                }
                else
                {
                    // We will be here when we disconnect from the server.
                    // So we can clear our state as well.
                    clearUpdateInfo();
                    setTargetState(WidgetUpdateState::initial, {});
                }
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

    ui->releaseNotesLabel->setText(QString("<a href='notes'>%1</a>").arg(tr("Release notes")));

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
        m_serverUpdateTool->setServerUrl(serverUrl, serverId);
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
            setUpdateSourceMode(UpdateSourceType::internetSpecific);
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
            QSet<QnUuid> targets = m_stateTracker->getAllPeers();
            auto url = generateUpdatePackageUrl(
                commonModule()->engineVersion(),
                m_updateInfo, targets,
                resourcePool()).toString();

            QDesktopServices::openUrl(url);
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            QSet<QnUuid> targets = m_stateTracker->getAllPeers();
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
    const nx::update::UpdateContents& contents)
{
    VersionReport report;

    using Error = nx::update::InformationError;
    using SourceType = nx::update::UpdateSourceType;

    bool validUpdate = contents.isValidToInstall();
    auto source = contents.sourceType;

    QString internalError = nx::update::toString(contents.error);
    // We have different error messages for each update source. So we should check
    // every combination of update source and nx::update::InformationError values.
    if (contents.alreadyInstalled && source != SourceType::internet)
    {
        report.versionMode = VersionReport::VersionMode::build;
        report.statusHighlight = VersionReport::HighlightMode::regular;
        report.statusMessages << tr("You have already installed this version.");
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
                    report.statusMessages << tr("Unable to check updates on the internet");
                break;
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
                auto missing = contents.missingUpdate.size()
                    + contents.unsuportedSystemsReport.size();
                if (contents.missingClientPackage)
                {
                    if (missing)
                    {
                        packageErrors << tr("Missing update package for the client and %n servers",
                            "", missing);
                    }
                    else
                    {
                        packageErrors << tr("Missing update package for the client");
                    }
                }
                else if (missing)
                {
                    packageErrors << tr("Missing update package for some servers");
                }

                report.versionHighlight = VersionReport::HighlightMode::bright;
                report.statusMessages << packageErrors;
                break;
            }
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

    QString versionRaw = nx::utils::AppInfo::applicationVersion();
    auto currentVersion = nx::utils::SoftwareVersion(versionRaw);

    if (validUpdate && contents.getVersion() < currentVersion)
        report.statusMessages << tr("Downgrade to earlier versions is not possible");

    return report;
}

void MultiServerUpdatesWidget::atUpdateCurrentState()
{
    NX_ASSERT(m_serverUpdateTool);

    if (isHidden())
        return;

    // We poll all our tools for new information. Then we update UI if there are any changes
    if (m_updateCheck.valid()
        && m_updateCheck.wait_for(kWaitForUpdateCheck) == std::future_status::ready)
    {
        auto checkResponse = m_updateCheck.get();
        NX_VERBOSE(this) << "atUpdateCurrentState got update info:"
            << checkResponse.info.version
            << "from" << checkResponse.source;
        if (checkResponse.sourceType == nx::update::UpdateSourceType::mediaservers)
        {
            NX_INFO(this) << "atUpdateCurrentState this is the data from /ec2/updateInformation";
        }

        m_serverUpdateTool->verifyUpdateManifest(checkResponse, m_clientUpdateTool->getInstalledVersions());
        if (m_updateInfo.preferOtherUpdate(checkResponse))
        {
            m_updateInfo = checkResponse;

            if (!m_updateInfo.unsuportedSystemsReport.empty())
                m_stateTracker->setVerificationError(checkResponse.unsuportedSystemsReport);

            if (!m_updateInfo.missingUpdate.empty())
            {
                m_stateTracker->setVerificationError(
                    checkResponse.missingUpdate, tr("No update package available"));
            }

            m_updateReport = calculateUpdateVersionReport(m_updateInfo);

            if (!m_updateInfo.clientPackage.isValid())
                syncStatusVisibility();
            m_haveValidUpdate = false;
            m_targetVersion = nx::utils::SoftwareVersion(checkResponse.info.version);
            m_targetChangeset = m_targetVersion.build();

            if (checkResponse.isValidToInstall() && !checkResponse.alreadyInstalled)
                m_haveValidUpdate = true;
        }
        else
        {
            NX_VERBOSE(NX_SCOPE_TAG, "current update info with version='%1' is better", m_updateInfo.info.version);
        }
        m_updateLocalStateChanged = true;
    }

    // Maybe we should not call it right here.
    m_serverUpdateTool->requestRemoteUpdateStateAsync();

    processRemoteChanges();
    processUploaderChanges();

    if (stateHasProgress(m_widgetState))
        syncProgress();
    if (m_serverUpdateTool->hasRemoteChanges() || m_updateLocalStateChanged)
        loadDataToUi();

    syncDebugInfoToUi();
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
    NX_INFO(this) << "clearUpdateInfo()";
    m_targetVersion = nx::utils::SoftwareVersion();
    m_updateReport = {};
    m_updateInfo = nx::update::UpdateContents();
    m_updatesModel->setUpdateTarget(nx::utils::SoftwareVersion());
    m_stateTracker->clearVerificationErrors();
    m_updateLocalStateChanged = true;
    m_updateCheck = std::future<nx::update::UpdateContents>();
}

void MultiServerUpdatesWidget::pickLocalFile()
{
    auto options = QnCustomFileDialog::fileDialogOptions();
    QString caption = tr("Select Update File...");
    QString filter = QnCustomFileDialog::createFilter(tr("Update Files"), "zip");
    QString fileName = QFileDialog::getOpenFileName(this, caption, QString(), filter, 0, options);

    if (fileName.isEmpty())
        return;

    m_updateSourceMode = UpdateSourceType::file;
    m_updateLocalStateChanged = true;

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
    m_updateLocalStateChanged = true;

    clearUpdateInfo();
    nx::utils::SoftwareVersion version = commonModule()->engineVersion();
    auto buildNumber = dialog.buildNumber();

    m_targetVersion = nx::utils::SoftwareVersion(version.major(), version.minor(), version.bugfix(), buildNumber);
    m_targetChangeset = dialog.changeset();
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
        auto installedVersions = m_clientUpdateTool->getInstalledVersions();
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
        // Maybe we should call loadDataToUi instead.
        syncUpdateCheckToUi();
    }
}

void MultiServerUpdatesWidget::setUpdateSourceMode(UpdateSourceType mode)
{
    if (m_updateSourceMode == mode)
        return;

    switch(mode)
    {
        case UpdateSourceType::internet:
            m_updateSourceMode = mode;
            m_updateLocalStateChanged = true;
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
        auto targets = m_stateTracker->getServersInState(StatusCode::readyToInstall);
        if (targets.empty() && !m_clientUpdateTool->hasUpdate())
        {
            NX_WARNING(this) << "atStartUpdateAction() - no server can install anything";
            return;
        }

        NX_INFO(this)
            << "atStartUpdateAction() - starting installation for"
            << targets.size() << ":" << targets;
        setTargetState(WidgetUpdateState::installing, targets);
    }
    else if (m_widgetState == WidgetUpdateState::ready && m_haveValidUpdate)
    {
        int acceptedEula = qnSettings->acceptedEulaVersion();
        int newEula = m_updateInfo.info.eulaVersion;
        const bool showEula = acceptedEula < newEula;

        if (showEula && EulaDialog::showEulaHtml(m_updateInfo.info.eula, mainWindowWidget())
            != QDialog::Accepted)
        {
            return;
        }

        auto targets = m_stateTracker->getAllPeers();

        // Remove the servers which should not be updated. These are the servers with more recent
        // version. So we should not track update progress for them.
        targets.subtract(m_updateInfo.ignorePeers);

        auto offlineServers = m_stateTracker->getOfflineServers();
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
            else
            {
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
        }

        auto incompatible = m_stateTracker->getLegacyServers();
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

        if (!m_clientUpdateTool->shouldInstallThis(m_updateInfo))
            targets.remove(m_stateTracker->getClientPeerId());

        m_stateTracker->setUpdateTarget(m_updateInfo.getVersion());
        m_stateTracker->markStatusUnknown(targets);

        if (m_updateSourceMode == UpdateSourceType::file)
        {
            setTargetState(WidgetUpdateState::pushing, targets);
            m_serverUpdateTool->startUpload(m_updateInfo);
        }
        else
        {
            // Start download process.
            // Move to state 'Downloading'
            // At this state should check remote state untill everything is complete.
            // TODO: We have the case when all mediaservers have installed update.

            setTargetState(WidgetUpdateState::downloading, targets);
            if (!m_updateInfo.manualPackages.empty())
                m_serverUpdateTool->startManualDownloads(m_updateInfo);
        }

        NX_INFO(this) << "atStartUpdateAction() - sending 'download' command to peers" << targets;
        m_serverUpdateTool->requestStartUpdate(m_updateInfo.info, targets);
        // We always run install commands for client. Though clientUpdateTool state can
        // fall through to 'readyRestart' or 'complete' state.
        m_clientUpdateTool->setUpdateTarget(m_updateInfo);
    }
    else
    {
        NX_WARNING(this) << "atStartUpdateAction() - invalid widget state for download command";
    }

    if (m_updateRemoteStateChanged)
        loadDataToUi();
}

bool MultiServerUpdatesWidget::atCancelCurrentAction()
{
    closePanelNotifications();

    // Cancel all the downloading.
    if (m_widgetState == WidgetUpdateState::downloading)
    {
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState);
        auto serversToCancel = m_peersIssued;
        m_serverUpdateTool->requestStopAction();
        m_clientUpdateTool->resetState();
        setTargetState(WidgetUpdateState::ready, {});
    }
    else if (m_widgetState == WidgetUpdateState::installingStalled)
    {
        // Should send 'cancel' command to all the servers?
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState);

        auto serversToCancel = m_stateTracker->getPeersInstalling();
        m_serverUpdateTool->requestFinishUpdate(true);
        m_clientUpdateTool->resetState();
        if (m_holdConnection)
        {
            qnClientMessageProcessor->setHoldConnection(false);
            m_holdConnection = false;
        }
        setTargetState(WidgetUpdateState::initial, {});
    }
    else if (m_widgetState == WidgetUpdateState::complete)
    {
        // Should send 'cancel' command to all the servers?
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState);

        auto serversToCancel = m_stateTracker->getPeersInstalling();
        m_serverUpdateTool->requestFinishUpdate(false);
        m_clientUpdateTool->resetState();
        setTargetState(WidgetUpdateState::initial, {});
    }
    else if (m_widgetState == WidgetUpdateState::readyInstall)
    {
        NX_VERBOSE(this) << "atCancelCurrentAction() at" << toString(m_widgetState);
        auto serversToCancel = m_stateTracker->getServersInState(StatusCode::readyToInstall);
        m_serverUpdateTool->requestStopAction();
        m_clientUpdateTool->resetState();
        setTargetState(WidgetUpdateState::ready, {});
    }
    else if (m_widgetState == WidgetUpdateState::pushing)
    {
        setTargetState(WidgetUpdateState::ready, {});
        m_serverUpdateTool->stopUpload();
        m_serverUpdateTool->requestStopAction();
    }
    else
    {
        NX_INFO(this) << "atCancelCurrentAction() at" << toString(m_widgetState) << ": not implemented";
        clearUpdateInfo();
        setUpdateSourceMode(UpdateSourceType::internet);
        checkForInternetUpdates();
        return false;
    }

    if (m_updateRemoteStateChanged)
        loadDataToUi();

    // Spec says that we can not cancel anything when we began installing stuff.
    return true;
}

void MultiServerUpdatesWidget::atCloudSettingsChanged()
{
    if (m_widgetState == WidgetUpdateState::ready)
    {
        if (!m_updateInfo.isEmpty())
        {
            NX_INFO(this, "atCloudSettingsChanged() - recalculating update report");
            auto installedVersions = m_clientUpdateTool->getInstalledVersions();
            if (m_updateInfo.error != nx::update::InformationError::httpError
                || m_updateInfo.error != nx::update::InformationError::networkError)
            {
                m_updateInfo.error = nx::update::InformationError::noError;
            }
            m_haveValidUpdate = m_serverUpdateTool->verifyUpdateManifest(m_updateInfo, installedVersions);
            m_updateReport = calculateUpdateVersionReport(m_updateInfo);
            m_updateLocalStateChanged = true;
        }
        else
        {
            NX_INFO(this, "atCloudSettingsChanged() - update info is completely empty. No reason to recalculate it");
        }
    }
    else
    {
        NX_INFO(this, "atCloudSettingsChanged() - no need to recalculate update info in the state %1", toString(m_widgetState));
    }

    if (m_updateLocalStateChanged)
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
    if (m_widgetState == WidgetUpdateState::downloading)
    {
        NX_INFO(this)
            << "atServerPackageDownloadFailed() - failed to download server package"
            << package.file << "error:" << error;
        for (const auto& id: package.targets)
            m_peersFailed.insert(id);
    }
    else
    {
        NX_INFO(this)
            << "atServerPackageDownloadFailed() - failed to download server package"
            << package.file << "and widget is not in downloading state" << "error:" << error;
    }
}

ServerUpdateTool::ProgressInfo MultiServerUpdatesWidget::calculateActionProgress() const
{
    ServerUpdateTool::ProgressInfo result;
    if (m_widgetState == WidgetUpdateState::pushing)
    {
        m_serverUpdateTool->calculateUploadProgress(result);
    }
    else if (m_widgetState == WidgetUpdateState::downloading)
    {
        // Note: we can get here right after we clicked 'download' but before
        // we got an update from /ec2/updateStatus. Most servers will be in 'idle' state.
        // We even can get a stale callback from /ec2/updateStatus, with data actual to
        // the moment right before we pressed 'Download'.
        // It will be very troublesome to properly wait for updated /ec2/updateStatus.
        for (const auto& id: m_peersIssued)
        {
            auto item = m_stateTracker->findItemById(id);
            if (!item)
                continue;
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
                case nx::update::Status::Code::offline:
                    break;
            }
        }

        // We sum up individual progress from every server and adjust maximum progress as well.
        // We could get 147 of 200 percents done. ProgressBar works fine with it.
        result.max += 100 * m_peersIssued.size();
        result.downloadingServers = !m_peersActive.empty();

        if (m_serverUpdateTool->hasManualDownloads())
            m_serverUpdateTool->calculateManualDownloadProgress(result);

        if (m_clientUpdateTool->hasUpdate())
            result.downloadingClient = !m_clientUpdateTool->isDownloadComplete();
    }
    else if (m_widgetState == WidgetUpdateState::installing
        || m_widgetState == WidgetUpdateState::installingStalled)
    {
        // Note: we can get here right after we clicked 'Install' but before
        // we get recent update from /ec2/updateStatus. Most servers will be in 'readyToInstall' state.
        // We even can get a stale callback from /ec2/updateStatus, with data actual to
        // the moment right before we pressed 'Install'.
        auto serversAreInstalling = m_stateTracker->getPeersInstalling();
        auto serversHaveInstalled = m_stateTracker->getPeersCompleteInstall();

        result.installingServers = !m_peersActive.empty();

        int total = m_peersIssued.size();

        auto installDuration = std::chrono::duration_cast<std::chrono::seconds>(
            m_serverUpdateTool->getInstallDuration());
        double phase = (double)installDuration.count() / kExpectedInstallPeriodSec;
        int fakeProgress = 85 * (1.0 - qPow(0.2, phase));
        result.current += qMax(serversHaveInstalled.size()*100, fakeProgress*serversAreInstalling.size());
        result.max += 100*total;

        if (m_clientUpdateTool->hasUpdate())
        {
            bool complete = m_clientUpdateTool->getState() == ClientUpdateTool::State::complete;
            result.installingClient = !complete;
        }
    }

    return result;
}

void MultiServerUpdatesWidget::processRemoteInitialState()
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

    setTargetState(WidgetUpdateState::checkingServers, {});
    // Maybe we should call loadDataToUi instead.
    syncUpdateCheckToUi();
}

void MultiServerUpdatesWidget::processRemoteUpdateInformation()
{
    if (!m_serverUpdateCheck.valid())
    {
        NX_ERROR(NX_SCOPE_TAG,
            "we were not waiting for /ec2/updateInformation for some reason. "
            "Reverting to initial state");
        setTargetState(WidgetUpdateState::initial, {});
        return;
    }

    if (
        m_serverUpdateCheck.wait_for(kWaitForUpdateCheck) == std::future_status::ready
        && m_serverStatusCheck.valid()
        && m_serverStatusCheck.wait_for(kWaitForUpdateCheck) == std::future_status::ready)
    {
        auto updateInfo = m_serverUpdateCheck.get();
        auto serverStatus = m_serverStatusCheck.get();

        ServerUpdateTool::RemoteStatus remoteStatus;
        m_serverUpdateTool->getServersStatusChanges(remoteStatus);
        m_stateTracker->setUpdateStatus(remoteStatus);

        /*
         * TODO: We should deal with starting client update.
         * There are two distinct situations:
         *  1. This is the client, that has initiated update process.
         *  2. This is 'other' client, that have found that an update process is running.
         *  It should download an update package using p2p downloader
         */
        auto installedVersions = m_clientUpdateTool->getInstalledVersions();
        if (!m_serverUpdateTool->verifyUpdateManifest(updateInfo, installedVersions)
            || !updateInfo.isValidToInstall())
        {
            // We can reach here when we reconnect to the server with complete updates.
            NX_INFO(NX_SCOPE_TAG,
                "processRemoteUpdateInformation() - there is no valid update info on mediaserver.");
            setTargetState(WidgetUpdateState::ready, {});
            return;
        }

        NX_INFO(NX_SCOPE_TAG, "mediaservers have an active update process to version %1", updateInfo.info.version);

        bool hasClientUpdate = m_clientUpdateTool->hasUpdate();
        if (updateInfo.alreadyInstalled && !hasClientUpdate)
        {
            // It seems like we should not change the state.
            NX_INFO(NX_SCOPE_TAG, "looks like we have installed this update already");
            setTargetState(WidgetUpdateState::ready, {});
            return;
        }

        // TODO: Client could have no update available for some reason. We should ignore it
        // if update process is already in 'installing' phase.
        if (m_updateInfo.preferOtherUpdate(updateInfo))
        {
            NX_INFO(NX_SCOPE_TAG, "taking update info from mediaserver");
            m_updateInfo = updateInfo;
            m_updateReport = calculateUpdateVersionReport(m_updateInfo);
            m_clientUpdateTool->setUpdateTarget(m_updateInfo);
            m_haveValidUpdate = true;
        }

        auto serversHaveDownloaded = m_stateTracker->getServersInState(StatusCode::readyToInstall);
        auto serversAreDownloading = m_stateTracker->getServersInState(StatusCode::downloading);
        auto peersAreInstalling = m_serverUpdateTool->getServersInstalling();
        auto serversHaveInstalled = m_stateTracker->getPeersCompleteInstall();

        m_updateLocalStateChanged = true;

        if (!peersAreInstalling.empty())
        {
            NX_INFO(this)
                << "processRemoteUpdateInformation() - servers" << peersAreInstalling << " are installing an update";
            // TODO: Should check if we need client update
            if (hasClientUpdate)
                peersAreInstalling.insert(m_stateTracker->getClientPeerId());
            setTargetState(WidgetUpdateState::installing, peersAreInstalling);
        }
        else if (!serversAreDownloading.empty())
        {
            NX_INFO(this)
                << "processRemoteUpdateInformation() - servers" << serversAreDownloading << "are downloading an update";
            setTargetState(WidgetUpdateState::downloading, serversAreDownloading);
        }
        else if (!serversHaveDownloaded.empty())
        {
            NX_INFO(this)
                << "processRemoteUpdateInformation() - servers" << serversHaveDownloaded << "have already downloaded an update";
            setTargetState(WidgetUpdateState::readyInstall, {});
        }
        else if (!serversHaveInstalled.empty())
        {
            NX_INFO(this)
                << "processRemoteUpdateInformation() - servers" << serversHaveInstalled
                << "have already installed an update";
            // We are here only if there are some offline servers and the rest of peers
            // have complete its update.
            // TODO: This state will be fixed later
            setTargetState(WidgetUpdateState::ready, {});
        }
        else
        {
            // We can reach here when we reconnect to the server with complete updates.
            NX_INFO(this)
                << "processRemoteUpdateInformation() - no servers in downloading/installing/downloaded/installed state."
                << "Update process seems to be stalled or complete. Ignoring this internal state.";
            setTargetState(WidgetUpdateState::ready, {});
        }

        loadDataToUi();
    }
}

void MultiServerUpdatesWidget::processRemoteDownloading()
{
    auto items = m_stateTracker->getAllItems();
    NX_ASSERT(!m_peersIssued.isEmpty());

    for (const auto& item: items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (!m_peersIssued.contains(id))
            continue;

        if (item->statusUnknown)
            continue;

        if (item->incompatible)
            continue;

        if (item->installed)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this)
                    << "processRemoteInstalling() - peer"
                    << id << "have already installed this package.";
                m_peersActive.remove(id);
            }
            m_peersComplete.insert(id);
        }
        else
        {
            switch (state)
            {
                case StatusCode::readyToInstall:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteDownloading() - peer"
                            << id << "completed downloading and is ready to install";
                        m_peersActive.remove(id);
                    }
                    m_peersComplete.insert(id);
                    break;
                case StatusCode::error:
                case StatusCode::idle:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteDownloading() - peer"
                            << id << "failed to download update package";
                        m_peersActive.remove(id);
                    }
                    m_peersFailed.insert(id);
                    break;
                case StatusCode::preparing:
                case StatusCode::downloading:
                    if (!m_peersActive.contains(id) && m_peersIssued.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteDownloading() - peer"
                            << id << "resumed downloading.";
                        m_peersActive.insert(id);
                    }
                    break;
                case StatusCode::offline:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteDownloading() - peer"
                            << id << "went offline during download.";
                        m_peersActive.remove(id);
                    }
                    break;
                case StatusCode::latestUpdateInstalled:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteInstalling() - peer"
                            << id << "have already installed this package.";
                        m_peersActive.remove(id);
                    }
                    m_peersComplete.insert(id);
                    break;
            }
        }
    }

    // No peers are doing anything. So we consider current state transition is complete
    if (m_peersActive.empty())
    {
        NX_INFO(this) << "processRemoteDownloading() - download is complete";

        // Need to sync UI before showing modal dialogs.
        // Or we will get inconsistent UI state in the background.
        loadDataToUi();

        if (m_peersComplete.size() >= m_peersIssued.size())
        {
            auto complete = m_peersComplete;
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Success);
            messageBox->setText(tr("Updates downloaded"));
            // S|Install now| |Later|
            setTargetState(WidgetUpdateState::readyInstall, complete);
            auto installNow = messageBox->addButton(tr("Install now"),
                QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
            auto installLater = messageBox->addButton(tr("Later"), QDialogButtonBox::RejectRole);
            messageBox->setEscapeButton(installLater);
            messageBox->exec();

            auto clicked = messageBox->clickedButton();
            if (clicked == installNow)
                setTargetState(WidgetUpdateState::installing, complete);
        }
        else if (m_peersComplete.empty())
        {
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 2. All servers have failed. Or no servers have succeded
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to download update packages"));

            QString text;
            text += htmlParagraph(tr("Please make sure there is enough free storage space and network connection is stable."));
            text += htmlParagraph(tr("If the problem persists, please contact Customer Support."));
            messageBox->setInformativeText(text);

            auto tryAgain = messageBox->addButton(tr("Try again"),
                QDialogButtonBox::AcceptRole);
            auto cancelUpdate = messageBox->addButton(tr("Cancel Update"),
                QDialogButtonBox::RejectRole);
            messageBox->setEscapeButton(cancelUpdate);
            messageBox->exec();

            auto clicked = messageBox->clickedButton();
            if (clicked == tryAgain)
            {
                auto serversToRetry = m_peersFailed;
                m_serverUpdateTool->requestStartUpdate(m_updateInfo.info, m_peersComplete);
                m_clientUpdateTool->setUpdateTarget(m_updateInfo);
                setTargetState(WidgetUpdateState::downloading, serversToRetry);
            }
            else if (clicked == cancelUpdate)
            {
                auto serversToStop = m_peersIssued;
                m_serverUpdateTool->requestStopAction();
                setTargetState(WidgetUpdateState::ready, {});
            }
        }
        else
        {
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 3. All other cases. Some servers have failed
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to download update packages to some servers"));

            QString text;
            text += htmlParagraph(tr("Please make sure they have enough free storage space and stable network connection."));
            text += htmlParagraph(tr("If the problem persists, please contact Customer Support."));
            messageBox->setInformativeText(text);

            auto tryAgain = messageBox->addButton(tr("Try again"),
                QDialogButtonBox::AcceptRole);
            auto cancelUpdate = messageBox->addButton(tr("Cancel Update"),
                QDialogButtonBox::RejectRole);
            messageBox->setEscapeButton(cancelUpdate);
            messageBox->exec();

            auto clicked = messageBox->clickedButton();
            if (clicked == tryAgain)
            {
                auto serversToRetry = m_peersFailed;
                m_serverUpdateTool->requestStartUpdate(m_updateInfo.info, serversToRetry);
                m_clientUpdateTool->setUpdateTarget(m_updateInfo);
                setTargetState(WidgetUpdateState::downloading, serversToRetry);
            }
            else if (clicked == cancelUpdate)
            {
                m_serverUpdateTool->requestStopAction();
                setTargetState(WidgetUpdateState::ready, {});
            }

            setTargetState(WidgetUpdateState::readyInstall, m_peersComplete);
        }
        // TODO: Check servers that are online, but skipped + m_serversCanceled
        // TODO: Show dialog "Some servers were skipped."
        {
            //messageBox->setText(tr("Only servers which have update packages downloaded will be updated."));
            //messageBox->setInformativeText(tr("Not updated servers may be unavailable if you are connected to the updated server, and vice versa."));
        }
    }
}

void MultiServerUpdatesWidget::processRemoteInstalling()
{
    auto duration = m_serverUpdateTool->getInstallDuration();
    if (m_widgetState == WidgetUpdateState::installing
        && duration > kLongInstallWarningTimeout)
    {
        NX_VERBOSE(this) << "processRemoteInstalling() - detected stalled installation";
        setTargetState(WidgetUpdateState::installingStalled, m_peersIssued);
    }

    auto items = m_stateTracker->getAllItems();
    QSet<QnUuid> peersToRestartUpdate;
    QSet<QnUuid> peersActive;
    QSet<QnUuid> readyToInstall;

    for (const auto& item: items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (item->incompatible)
            continue;
        //if (!m_peersIssued.contains(id))
        //    continue;
        if (item->installed)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this)
                    << "processRemoteInstalling() - peer"
                    << id << "has already installed this package.";
                m_peersActive.remove(id);
            }
            m_peersComplete.insert(id);
        }
        else if (item->installing)
        {
            if (!m_peersActive.contains(id))
            {
                NX_VERBOSE(this)
                    << "processRemoteInstalling() - peer"
                    << id << "has resumed installation";
                m_peersActive.insert(id);
                peersToRestartUpdate.insert(id);
            }
            if (item->component == UpdateItem::Component::server)
            {
                peersActive.insert(item->id);
            }
        }
        else
        {
            switch (state)
            {
                case StatusCode::readyToInstall:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteInstalling() - peer "
                            << id << "completed downloading and is ready to install";
                        m_peersActive.remove(id);
                        // TODO: Should force installing for this peers, until it is a client.
                        // Client should wait until servers have completed installation.
                        peersToRestartUpdate.insert(id);
                    }
                    readyToInstall.insert(id);
                    break;
                case StatusCode::error:
                case StatusCode::idle:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteInstalling() - peer"
                            << id << "failed to download update package";
                        m_peersFailed.insert(id);
                        m_peersActive.remove(id);
                    }
                    break;
                case StatusCode::preparing:
                case StatusCode::downloading:
                    if (!m_peersActive.contains(id)/* && m_peersIssued.contains(id)*/)
                    {
                        NX_VERBOSE(this)
                            << "processRemoteInstalling() - peer"
                            << id << "resumed downloading.";
                        m_peersActive.insert(id);
                    }
                    break;
                case StatusCode::latestUpdateInstalled:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processRemoteInstalling() - peer"
                            << id << "have already installed this package.";
                        m_peersActive.remove(id);
                    }
                    m_peersComplete.insert(id);
                    break;
                case StatusCode::offline:
                    break;
            }
        }
    }

    // We need the most recent version information only in state 'installing'.
    m_serverUpdateTool->requestModuleInformation();

    // No peers are doing anything right now. We should check if installation is complete.
    if (peersActive.empty())
    {
        if (!m_peersComplete.empty())
        {
            NX_INFO(this) << "processRemoteInstalling() - installation is complete";
            setTargetState(WidgetUpdateState::complete);
            loadDataToUi();

            auto complete = m_peersComplete;
            QScopedPointer<QnMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Success);

            if (m_peersFailed.empty())
            {
                messageBox->setText(tr("Update completed"));
            }
            else
            {
                NX_ERROR(this) << "processRemoteInstalling() - servers" << m_peersFailed << " have failed to install update";
                messageBox->setText(tr("Update completed, but some servers have failed an update"));
                injectResourceList(*messageBox, resourcePool()->getResourcesByIds(m_peersFailed));
            }

            if (m_clientUpdateTool->shouldRestartTo(m_updateInfo.getVersion()))
                messageBox->setInformativeText(tr("Nx Witness Client will be restarted to the updated version."));

            messageBox->addButton(tr("OK"),
                QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
            messageBox->exec();

            bool shouldRestartClient = m_clientUpdateTool->hasUpdate()
                && m_clientUpdateTool->shouldRestartTo(m_updateInfo.getVersion());
            completeInstallation(shouldRestartClient);
        }
        // No servers have installed updates
        else if (m_peersComplete.empty() && !m_peersFailed.empty())
        {
            NX_ERROR(this) << "processRemoteInstalling() - installation has failed completely";
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to install updates to servers:"));
            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(m_peersFailed));
            QString text;
            text += htmlParagraph(tr("Please make sure there is enough free storage space and network connection is stable."));
            text += htmlParagraph(tr("If the problem persists, please contact Customer Support."));
            messageBox->setInformativeText(text);
            auto installNow = messageBox->addButton(tr("OK"),
                QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
            messageBox->setEscapeButton(installNow);
            messageBox->exec();
            m_serverUpdateTool->requestStopAction();
            setTargetState(WidgetUpdateState::initial);
            loadDataToUi();
        }
    }
    if (!readyToInstall.empty())
    {
        auto clientId = m_stateTracker->getClientPeerId();
        if (readyToInstall.contains(clientId))
            m_clientUpdateTool->installUpdateAsync();
        // We should not do anything with servers in this state.
    }
}

void MultiServerUpdatesWidget::completeInstallation(bool clientUpdated)
{
    auto updatedProtocol = m_stateTracker->getServersWithChangedProtocol();
    bool clientInstallerRequired = false;
    m_serverUpdateTool->requestFinishUpdate(true);

    if (clientUpdated && !clientInstallerRequired)
    {
        QString authString = m_serverUpdateTool->getServerAuthString();
        NX_INFO(this) << "completeInstallation() - restarting the client";
        if (!m_clientUpdateTool->restartClient(authString))
        {
            NX_ERROR(this) << "completeInstallation(" << clientUpdated << ") - failed to run restart command";
            QnConnectionDiagnosticsHelper::failedRestartClientMessage(this);
        }
        else
        {
            applauncher::api::scheduleProcessKill(QCoreApplication::applicationPid(), kProcessTerminateTimeoutMs);
            qApp->exit(0);
            return;
        }
    }

    if (m_holdConnection)
    {
        qnClientMessageProcessor->setHoldConnection(false);
        m_holdConnection = false;
    }

    if (!updatedProtocol.empty())
    {
        NX_INFO(this) << "completeInstallation() - servers" << updatedProtocol << "have new protocol. Forcing reconnect";
        menu()->trigger(action::DisconnectAction, {Qn::ForceRole, true});
    }

    setTargetState(WidgetUpdateState::initial);
}

bool MultiServerUpdatesWidget::processRemoteChanges()
{
    // We gather here updated server status from updateTool
    // and change WidgetUpdateState state accordingly.

    // TODO: It could be moved to UpdateTool
    ServerUpdateTool::RemoteStatus remoteStatus;
    if (m_serverUpdateTool->getServersStatusChanges(remoteStatus))
        m_stateTracker->setUpdateStatus(remoteStatus);

    m_clientUpdateTool->checkInternalState();

    if (m_widgetState == WidgetUpdateState::initial)
        processRemoteInitialState();

    if (m_widgetState == WidgetUpdateState::checkingServers)
        processRemoteUpdateInformation();

    if (m_widgetState == WidgetUpdateState::downloading)
    {
        processRemoteDownloading();
    }
    else if (m_widgetState == WidgetUpdateState::installing
        || m_widgetState == WidgetUpdateState::installingStalled)
    {
        processRemoteInstalling();
    }
    else if (m_widgetState == WidgetUpdateState::readyInstall)
    {
        auto idle = m_stateTracker->getServersInState(StatusCode::idle);
        auto all = m_stateTracker->getAllPeers();
        if (idle.size() == all.size() && m_serverUpdateTool->haveActiveUpdate())
        {
            setTargetState(WidgetUpdateState::ready, {});
        }
    }
    m_updateRemoteStateChanged = true;

    return true;
}

bool MultiServerUpdatesWidget::processUploaderChanges(bool force)
{
    if (!m_serverUpdateTool->hasOfflineUpdateChanges() && !force)
        return false;
    auto state = m_serverUpdateTool->getUploaderState();

    if (m_widgetState == WidgetUpdateState::pushing)
    {
        if (state == ServerUpdateTool::OfflineUpdateState::done)
        {
            NX_INFO(this) << "processUploaderChanges seems to be done";
            setTargetState(WidgetUpdateState::readyInstall, {});
        }
        else if (state == ServerUpdateTool::OfflineUpdateState::error)
        {
            NX_INFO(this) << "processUploaderChanges failed to upload all packages";
            setTargetState(WidgetUpdateState::ready, {});
        }
    }
    return true;
}

void MultiServerUpdatesWidget::setTargetState(WidgetUpdateState state, QSet<QnUuid> targets)
{
    if (state != m_widgetState)
    {
        NX_VERBOSE(this) << "setTargetState() from"
            << toString(m_widgetState)
            << "to"
            << toString(state);
        bool stopProcess = false;
        switch (state)
        {
            case WidgetUpdateState::initial:
                stopProcess = true;
                break;
            case WidgetUpdateState::ready:
                stopProcess = true;
                break;
            case WidgetUpdateState::downloading:
                if (m_rightPanelDownloadProgress.isNull() && ini().systemUpdateProgressInformers)
                {
                    auto manager = context()->instance<WorkbenchProgressManager>();
                    m_rightPanelDownloadProgress = manager->add(tr("Downloading updates..."));
                }
                break;
            case WidgetUpdateState::pushing:
                if (m_rightPanelDownloadProgress.isNull() && ini().systemUpdateProgressInformers)
                {
                    auto manager = context()->instance<WorkbenchProgressManager>();
                    m_rightPanelDownloadProgress = manager->add(tr("Pushing updates..."));
                }
                break;
            case WidgetUpdateState::readyInstall:
                stopProcess = true;
                break;
            case WidgetUpdateState::installingStalled:
                break;
            case WidgetUpdateState::installing:
                if (!targets.empty())
                {
                    m_stateTracker->setPeersInstalling(targets, true);
                    QSet<QnUuid> servers = targets;
                    servers.remove(m_stateTracker->getClientPeerId());
                    if (!servers.empty() && !m_holdConnection)
                    {
                        m_holdConnection = true;
                        qnClientMessageProcessor->setHoldConnection(true);
                    }
                    m_serverUpdateTool->requestInstallAction(servers);
                }

                if (m_clientUpdateTool->hasUpdate())
                    m_clientUpdateTool->installUpdateAsync();
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
    m_peersActive = targets;
    m_peersIssued = targets;
    m_peersComplete = {};
    m_peersFailed = {};
    m_updateRemoteStateChanged = true;
    m_updateLocalStateChanged = true;
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
    return m_updateCheck.valid()
        || m_serverUpdateCheck.valid()
        || m_widgetState == WidgetUpdateState::initial;
}

void MultiServerUpdatesWidget::syncUpdateCheckToUi()
{
    const bool isChecking = this->isChecking();
    const bool hasEqualUpdateInfo = m_stateTracker->lowestInstalledVersion() >= m_updateInfo.getVersion();
    bool hasLatestVersion = false;
    if (m_updateInfo.isEmpty() || m_updateInfo.error == nx::update::InformationError::noNewVersion)
        hasLatestVersion = true;
    else if (hasEqualUpdateInfo || m_updateInfo.alreadyInstalled)
        hasLatestVersion = true;
    //if (m_updateInfo.isValid() && m_updateInfo.sourceType != UpdateSourceType::internet)
    //    hasLatestVersion = false;

    if (m_updateInfo.error != nx::update::InformationError::noNewVersion
        && m_updateInfo.error != nx::update::InformationError::noError)
    {
        hasLatestVersion = false;
    }

    if (m_widgetState != WidgetUpdateState::ready
        && m_widgetState != WidgetUpdateState::initial
        && hasLatestVersion)
    {
        NX_DEBUG(this) << "syncUpdateCheck() dropping hasLatestVersion flag";
        hasLatestVersion = false;
    }

    ui->cancelProgressAction->setEnabled(m_widgetState != WidgetUpdateState::installing);

    if (m_widgetState == WidgetUpdateState::installingStalled)
        ui->cancelProgressAction->setText(tr("Finish Update"));
    else
        ui->cancelProgressAction->setText(tr("Cancel"));

    ui->selectUpdateTypeButton->setEnabled(!isChecking);

    if (isChecking)
    {
        ui->versionStackedWidget->setCurrentWidget(ui->checkingPage);
        ui->updateStackedWidget->setCurrentWidget(ui->emptyPage);
        ui->updateCheckMode->setVisible(false);
        ui->releaseDescriptionLabel->setText(QString());
        ui->errorLabel->setText(QString());
        hasLatestVersion = false;
    }
    else
    {
        ui->downloadButton->setVisible(m_haveValidUpdate);
        if (hasLatestVersion)
        {
            if (m_updateInfo.sourceType == UpdateSourceType::internet)
                ui->latestVersionBannerLabel->setText(tr("The latest version is already installed"));
            else
                ui->latestVersionBannerLabel->setText(tr("This version is already installed"));

            ui->versionStackedWidget->setCurrentWidget(ui->latestVersionPage);
            ui->updateStackedWidget->setCurrentWidget(ui->emptyPage);
        }
        else
        {
            if (m_haveValidUpdate)
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

                ui->releaseDescriptionLabel->setText(m_updateInfo.info.description);
                ui->releaseDescriptionLabel->setVisible(!m_updateInfo.info.description.isEmpty());
            }

            ui->versionStackedWidget->setCurrentWidget(ui->versionPage);
            ui->updateStackedWidget->setCurrentWidget(ui->updateControlsPage);
        }

        if (!m_haveValidUpdate)
            ui->releaseDescriptionLabel->setText(QString());

        bool browseUpdateVisible = false;
        if (/*!m_haveValidUpdate && */m_widgetState != WidgetUpdateState::readyInstall)
        {
            if (m_updateSourceMode == UpdateSourceType::internetSpecific)
            {
                browseUpdateVisible = true;
                ui->browseUpdate->setText(tr("Select Another Build"));
            }
            else if (m_updateSourceMode == UpdateSourceType::file)
            {
                browseUpdateVisible = true;
                ui->browseUpdate->setText(tr("Browse for Another File..."));
            }
            else if (hasLatestVersion)
            {
                browseUpdateVisible = true;
                ui->browseUpdate->setText(tr("Update to Specific Build"));
            }
        }
        ui->browseUpdate->setVisible(browseUpdateVisible);
        ui->latestVersionIconLabel->setVisible(hasLatestVersion);
        ui->updateCheckMode->setVisible(m_updateSourceMode == UpdateSourceType::internet);
        m_updatesModel->setUpdateTarget(m_updateInfo.getVersion());
    }

    ui->selectUpdateTypeButton->setText(toString(m_updateSourceMode));

    const bool showButton = !isChecking && !hasLatestVersion
        && m_updateSourceMode != UpdateSourceType::file
        && (m_widgetState == WidgetUpdateState::ready
            || m_widgetState != WidgetUpdateState::initial)
        && (m_updateInfo.error == nx::update::InformationError::networkError
            || m_updateInfo.error == nx::update::InformationError::httpError
            // If one wants to download a file in another place.
            || m_updateInfo.error == nx::update::InformationError::noError);

    ui->manualDownloadButton->setVisible(showButton);

    syncVersionReport(m_updateReport);
    m_updateLocalStateChanged = false;
}

bool MultiServerUpdatesWidget::stateHasProgress(WidgetUpdateState state)
{
    switch (state)
    {
        case WidgetUpdateState::initial:
        case WidgetUpdateState::checkingServers:
        case WidgetUpdateState::ready:
        case WidgetUpdateState::readyInstall:
        case WidgetUpdateState::complete:
            return false;
        case WidgetUpdateState::downloading:
        case WidgetUpdateState::installing:
        case WidgetUpdateState::installingStalled:
        case WidgetUpdateState::pushing:
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

    ui->updateStackedWidget->setCurrentWidget(ui->updateProgressPage);

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
    // This function gathers state status of update from remote servers and changes
    // UI state accordingly.
    syncStatusVisibility();
    // Title to be shown for this UI state.
    QString updateTitle;

    bool storageSettingsVisible = false;

    switch (m_widgetState)
    {
        case WidgetUpdateState::initial:
            storageSettingsVisible = true;
            break;
        case WidgetUpdateState::ready:
            if (m_haveValidUpdate)
                storageSettingsVisible = true;
            break;
        case WidgetUpdateState::downloading:
            updateTitle = tr("Updating to ...");
            break;
        case WidgetUpdateState::readyInstall:
            updateTitle = tr("Ready to update to");
            ui->downloadButton->setText(tr("Install update"));
            break;
        case WidgetUpdateState::installing:
            updateTitle = tr("Updating to ...");
            break;
        case WidgetUpdateState::pushing:
            break;
        case WidgetUpdateState::complete:
            updateTitle = tr("System updated to");
            ui->downloadButton->setText(tr("Install update"));
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

    // Should we show progress for this UI state.
    if (isChecking())
        ui->updateStackedWidget->setCurrentWidget(ui->emptyPage);
    else if (stateHasProgress(m_widgetState))
        syncProgress();
    else if (m_widgetState == WidgetUpdateState::complete)
        ui->updateStackedWidget->setCurrentWidget(ui->emptyPage);
    else
        ui->updateStackedWidget->setCurrentWidget(ui->updateControlsPage);

    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn, !m_showStorageSettings);
    QString icon = m_showStorageSettings
        ? QString("text_buttons/expand.png")
        : QString("text_buttons/collapse.png");
    ui->advancedUpdateSettings->setIcon(qnSkin->icon(icon));
    if (m_showStorageSettings)
        ui->tableView->setEditTriggers(QAbstractItemView::AllEditTriggers);
    else
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //NX_VERBOSE(this) << "syncRemoteUpdateState() - done with m_updateStateChanged";
    m_updateRemoteStateChanged = false;
}

void MultiServerUpdatesWidget::loadDataToUi()
{
    // Synchronises internal state and UI widget state.
    NX_ASSERT(m_serverUpdateTool);

    // Update UI state to match modes: {SpecificBuild;LatestVersion;LocalFile}
    if (m_updateLocalStateChanged || m_updateRemoteStateChanged)
    {
        // This one depends both on local and remote information.
        syncUpdateCheckToUi();
    }

    if (m_updateRemoteStateChanged)
        syncRemoteUpdateStateToUi();

    bool endOfTheWeek = QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek;
    ui->dayWarningBanner->setVisible(endOfTheWeek);

    // TODO: Update logic for auto check
    //setAutoUpdateCheckMode(qnGlobalSettings->isUpdateNotificationsEnabled());

    syncDebugInfoToUi();
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StatusMessageColumn, !m_showDebugData);

    syncVersionInfoVisibility();

    if (auto layout = ui->versionStackedWidget->currentWidget()->layout(); NX_ASSERT(layout))
        layout->activate();

    ui->controlsVerticalLayout->activate();
}

void MultiServerUpdatesWidget::syncVersionInfoVisibility()
{
    const bool hasError = !ui->errorLabel->text().isEmpty();
    ui->errorLabel->setVisible(hasError);

    const bool hasReleaseDescription = !ui->releaseDescriptionLabel->text().isEmpty();
    ui->releaseDescriptionHolder->setVisible(hasReleaseDescription && !hasError);

    const bool hasReleaseNotes = !m_updateInfo.info.releaseNotesUrl.isEmpty();
    ui->releaseNotesLabel->setVisible(hasReleaseNotes && !hasError);
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
            QString("validUpdate=%1").arg(m_haveValidUpdate),
            QString("targetVersion=%1").arg(m_updateInfo.info.version),
            QString("checkUpdate=%1").arg(m_updateCheck.valid()),
            QString("checkServerUpdate=%1").arg(m_serverUpdateCheck.valid()),
            QString("<a href=\"%1\">/ec2/updateStatus</a>").arg(m_serverUpdateTool->getUpdateStateUrl()),
            QString("<a href=\"%1\">/ec2/updateInformation</a>").arg(m_serverUpdateTool->getUpdateInformationUrl()),
            QString("<a href=\"%1\">/ec2/updateInformation?version=installed</a>").arg(m_serverUpdateTool->getInstalledUpdateInfomationUrl()),
        };

        debugState << QString("lowestVersion=%1").arg(m_stateTracker->lowestInstalledVersion().toString());

        debugState << QString("size(peers)=%1").arg(m_stateTracker->peersCount());
        debugState << QString("size(model)=%1").arg(m_updatesModel->rowCount());
        debugState << QString("size(sorted)=%1").arg(m_sortedModel->rowCount());

        QStringList serversReport;
        for (auto server: m_serverUpdateTool->getServersInstalling())
            serversReport << server.toString();
        if (!serversReport.empty())
            debugState << QString("installing=%1").arg(serversReport.join(","));

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

void MultiServerUpdatesWidget::syncStatusVisibility()
{
    using StatusMode = ServerStatusItemDelegate::StatusMode;
    StatusMode statusMode = StatusMode::remoteStatus;

    if (m_widgetState == WidgetUpdateState::initial
        || m_widgetState == WidgetUpdateState::ready)
    {
        statusMode = StatusMode::hidden;
    }

    if (m_stateTracker->hasVerificationErrors())
        statusMode = StatusMode::reportErrors;
    else if (m_stateTracker->hasStatusErrors())
        statusMode = StatusMode::remoteStatus;
    else if (m_updateInfo.alreadyInstalled)
        statusMode = StatusMode::hidden;

    m_statusItemDelegate->setStatusMode(statusMode);
    // This will force delegate update all its widgets.
    ui->tableView->update();
    m_updatesModel->forceUpdateColumn(ServerUpdatesModel::Columns::ProgressColumn);
    //forceUpdateColumn()
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::ProgressColumn,
        statusMode == StatusMode::hidden);
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

QString MultiServerUpdatesWidget::toString(LocalStatusCode status)
{
    // These strings are internal and are not intended to be visible to the user.
    switch (status)
    {
        case LocalStatusCode::downloading:
            return "Downloading";
        case LocalStatusCode::preparing:
            return "Preparing";
        case LocalStatusCode::readyToInstall:
            return "ReadyToInstall";
        case LocalStatusCode::error:
            return "Error";
        default:
            NX_ASSERT(false);
            return "UnknowState";
    }
}

QString MultiServerUpdatesWidget::toString(WidgetUpdateState state)
{
    // These strings are internal and are not intended to be visible to regular user.
    switch (state)
    {
        case WidgetUpdateState::initial:
            return "Initial";
        case WidgetUpdateState::checkingServers:
            return "CheckServers";
        case WidgetUpdateState::ready:
            return "Ready";
        case WidgetUpdateState::downloading:
            return "RemoteDownloading";
        case WidgetUpdateState::pushing:
            return "LocalPushing";
        case WidgetUpdateState::readyInstall:
            return "ReadyInstall";
        case WidgetUpdateState::installing:
            return "Installing";
        case WidgetUpdateState::installingStalled:
            return "InstallationStalled";
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

} // namespace nx::vms::client::desktop
