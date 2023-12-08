// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multi_server_updates_widget.h"
#include "ui_multi_server_updates_widget.h"

#include <functional>

#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMenu>

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/utils/app_info.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/client/desktop/system_update/version_selection_dialog.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/eula_dialog.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/update/tools.h>
#include <nx_ec/abstract_ec_connection.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <utils/applauncher_utils.h>
#include <utils/connection_diagnostics_helper.h>

#include "client/client_globals.h"
#include "peer_state_tracker.h"
#include "server_status_delegate.h"
#include "server_update_tool.h"
#include "server_updates_model.h"
#include "workbench_update_watcher.h"

using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::common;

namespace {

const auto kLongInstallWarningTimeout = std::chrono::minutes(2);
const int kTooLateDayOfWeek = Qt::Thursday;
const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour
const int kVersionLabelFontSizePixels = 24;
const auto kVersionLabelFontWeight = QFont::Medium;

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

/* N-dash 5 times: */
const QString kNoVersionNumberText = QString(5, nx::UnicodeChars::kEnDash);

constexpr int kVersionSelectionBlockHeight = 20;
constexpr int kVersionInformationBlockMinHeight = 48;
constexpr int kPreloaderHeight = 32;

static const QColor kLight4Color = "#E1E7EA";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kNormalIconSubstitutions = {
    {QIcon::Normal, {{kLight4Color, "light4"}}},
};

static const QColor kLight10Color = "#A5B7C0";
static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light10"}, {kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight10Color, "light11"}, {kLight16Color, "light17"}}},
    {QIcon::Selected, {{kLight16Color, "light15"}}},
};

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

    // Adding to `BeforeAdditionalInfo` looks much better than to the main layout.
    messageBox.addCustomWidget(resourceList, QnMessageBox::Layout::BeforeAdditionalInfo);
    return resourceList;
}

void toDebugString(QStringList& lines, const QString& caption, const QSet<QnUuid>& uids)
{
    QStringList report;
    for (auto id: uids)
        report << id.toString();

    if (!report.empty())
    {
        if (report.length() < 10)
            lines << QString("%1=%2").arg(caption, report.join(", "));
        else
            lines << QString("%1=%2 peers").arg(caption, QString::number(report.length()));
    }
}

QString getComponentQuery(const QSet<nx::utils::OsInfo>& serverInfoSet)
{
    const auto makeItem =
        [](const QString& component, const nx::utils::OsInfo& info)
        {
            QJsonObject obj = info.toJson();
            obj["component"] = component;
            return obj;
        };

    QJsonArray json{makeItem("client", nx::utils::OsInfo::current())};
    for (const auto& info: serverInfoSet)
        json.append(makeItem("server", info));

    const QByteArray& data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QByteArray compressed = qCompress(data);
    compressed.remove(0, 4); //< We don't need 4 bytes which are added by Qt in the beginning.
    return QString::fromLatin1(compressed.toBase64());
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

    auto watcher = context()->instance<nx::vms::client::desktop::WorkbenchUpdateWatcher>();
    m_serverUpdateTool = watcher->getServerUpdateTool();
    NX_ASSERT(m_serverUpdateTool);

    m_clientUpdateTool.reset(new ClientUpdateTool(systemContext(), this));
    m_updateCheck = watcher->takeUpdateCheck();

    m_stateTracker = m_serverUpdateTool->getStateTracker();
    m_updatesModel.reset(new ServerUpdatesModel(m_stateTracker, this));

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
    ui->latestVersionIconLabel->setPixmap(
        qnSkin->icon("system_settings/update_checkmark_40.svg", kNormalIconSubstitutions)
            .pixmap(QSize(40, 40)));
    ui->linkCopiedIconLabel->setPixmap(
        qnSkin->icon("text_buttons/ok_20.svg", kIconSubstitutions).pixmap(QSize(20, 20)));
    ui->linkCopiedWidget->hide();

    setHelpTopic(this, HelpTopic::Id::Administration_Update);

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
            if (m_updateSourceMode == UpdateSourceType::internet)
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

    ui->advancedUpdateSettings->setIcon(qnSkin->icon("text_buttons/arrow_up_20.svg", kIconSubstitutions));

    setAccentStyle(ui->downloadButton);
    // This button is hidden for now. We will implement it in future.
    ui->downloadAndInstall->hide();

    connect(ui->releaseNotesUrl, &QLabel::linkActivated, this,
        [this]()
        {
            if (m_updateInfo.isValidToInstall() && !m_updateInfo.info.releaseNotesUrl.isEmpty())
                QDesktopServices::openUrl(m_updateInfo.info.releaseNotesUrl);
        });

    connect(systemSettings(), &SystemSettings::cloudSettingsChanged, this,
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

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::packageDownloaded,
        this, &MultiServerUpdatesWidget::atServerPackageDownloaded);

    connect(m_serverUpdateTool.data(), &ServerUpdateTool::packageDownloadFailed,
        this, &MultiServerUpdatesWidget::atServerPackageDownloadFailed);

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

    connect(systemSettings(), &SystemSettings::localSystemIdChanged, this,
        [this]()
        {
            auto systemId = systemSettings()->localSystemId();
            NX_DEBUG(this, "localSystemId is changed to %1. Need to refresh server list", systemId);
            if (systemId.isNull())
            {
                NX_DEBUG(this, "localSystemIdChanged() we have disconnected. Detaching resource pool");
                m_serverUpdateTool->saveInternalState();
                m_stateTracker->setResourceFeed(nullptr);
                return;
            }
            else if (m_stateTracker && m_stateTracker->setResourceFeed(resourcePool()))
            {
                // We will be here when we connected to another system.
                // We should run update check again. This should fix VMS-13037.
                if (m_widgetState == WidgetUpdateState::initial && !m_updateCheck.valid())
                {
                    m_updateCheck = m_serverUpdateTool->checkForUpdate(
                        update::LatestVmsVersionParams{appContext()->version()});
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

    static_assert(kTooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
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

    ui->manualDownloadButton->hide();
    ui->manualDownloadButton->setIcon(
        qnSkin->icon("text_buttons/download_20.svg", kIconSubstitutions));
    ui->manualDownloadButton->setForegroundRole(QPalette::WindowText);

    ui->checkAgainButton->hide();
    ui->checkAgainButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));
    ui->checkAgainButton->setForegroundRole(QPalette::WindowText);

    ui->tryAgainButton->hide();
    ui->tryAgainButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));
    ui->tryAgainButton->setForegroundRole(QPalette::WindowText);

    ui->advancedSettingsButton->setIcon(
        qnSkin->icon("text_buttons/settings_20.svg", kIconSubstitutions));
    ui->advancedSettingsButton->setForegroundRole(QPalette::WindowText);
    connect(ui->advancedSettingsButton,
        &QPushButton::clicked,
        this,
        &MultiServerUpdatesWidget::openAdvancedSettings);

    initDownloadActions();
    initDropdownActions();

    // Starting timer that tracks remote update state
    m_stateCheckTimer.reset(new QTimer(this));
    m_stateCheckTimer->setSingleShot(false);
    m_stateCheckTimer->start(1000);
    connect(m_stateCheckTimer.get(), &QTimer::timeout,
        this, &MultiServerUpdatesWidget::atUpdateCurrentState);

    if (const auto connection = systemContext()->messageBusConnection())
    {
        const nx::vms::api::ModuleInformation& moduleInformation = connection->moduleInformation();

        m_clientUpdateTool->setServerUrl(
            core::LogonData{
                .address = connection->address(),
                .credentials = connection->credentials(),
                .expectedServerId = moduleInformation.id,
                .expectedServerVersion = moduleInformation.version,
            },
            systemContext()->certificateVerifier());
        //m_clientUpdateTool->requestRemoteUpdateInfo();
    }
    // Force update when we open dialog.
    checkForInternetUpdates(/*initial=*/true);

    loadDataToUi();
}

MultiServerUpdatesWidget::~MultiServerUpdatesWidget()
{
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
}

void MultiServerUpdatesWidget::initDownloadActions()
{
    auto downloadLinkMenu = new QMenu(this);
    downloadLinkMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    downloadLinkMenu->addAction(tr("Download in External Browser"),
        [this]()
        {
            QDesktopServices::openUrl(generateUpcombinerUrl().toQUrl());
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            qApp->clipboard()->setText(generateUpcombinerUrl().toString());

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

    connect(ui->checkAgainButton, &QPushButton::clicked, this,
        [this]()
        {
            if (m_updateSourceMode == UpdateSourceType::internet)
                checkForInternetUpdates();
            else if (m_updateSourceMode == UpdateSourceType::internetSpecific)
                recheckSpecificBuild();
        });

    connect(ui->tryAgainButton, &QPushButton::clicked, this,
        [this]()
        {
            if (m_updateSourceMode == UpdateSourceType::internetSpecific)
                recheckSpecificBuild();
            else
                checkForInternetUpdates();
        });
}

MultiServerUpdatesWidget::VersionReport MultiServerUpdatesWidget::calculateUpdateVersionReport(
    const UpdateContents& contents, QnUuid clientId)
{
    VersionReport report;

    using Error = common::update::InformationError;

    bool validUpdate = contents.isValidToInstall();
    auto source = contents.sourceType;

    // We have different error messages for each update source. So we should check
    // every combination of update source and common::update::InformationError values.
    if (contents.alreadyInstalled
        && (source == UpdateSourceType::internet)
            == (contents.error == Error::incompatibleVersion))
    {
        report.versionMode = VersionReport::VersionMode::build;
        report.statusHighlight = VersionReport::HighlightMode::regular;
        report.statusMessages << tr("You have already installed this version.");

        if (source == UpdateSourceType::internet)
            report.alreadyInstalledMessage = tr("The latest version is already installed");
        else
            report.alreadyInstalledMessage = tr("This version is already installed");

        report.hasLatestVersion = true;
    }
    else if (contents.error == Error::noError)
    {
        report.version = contents.info.version.toString();
        return report;
    }
    else if (!validUpdate)
    {
        report.versionHighlight = VersionReport::HighlightMode::red;
        report.statusHighlight = VersionReport::HighlightMode::red;
        // Note: it is possible to have multiple problems. Some problems can be tracked by
        // both error code and some error flag. So some error statuses are filled separately
        // from this switch.
        switch (contents.error)
        {
            case Error::emptyPackagesUrls:
            case Error::noError:
                // We should not be here.
                break;
            case Error::networkError:
                // Unable to check update from the Internet.
                report.statusMessages << tr("Unable to check updates on the Internet");
                report.versionMode = VersionReport::VersionMode::empty;
                report.versionHighlight = VersionReport::HighlightMode::regular;
                break;
            case Error::httpError:
                NX_ASSERT(source == UpdateSourceType::internet
                    || source == UpdateSourceType::internetSpecific);

                if (source == UpdateSourceType::internetSpecific)
                {
                    report.versionMode = VersionReport::VersionMode::build;
                    report.statusMessages << tr("Build not found");
                }
                else
                {
                    report.versionMode = VersionReport::VersionMode::empty;
                    report.statusMessages << tr("Unable to check updates on the Internet");
                }
                break;
            case Error::incompatibleParser:
                NX_ERROR(NX_SCOPE_TAG, "A proper update description file is not found.");
                [[fallthrough]];
            case Error::jsonError:
                if (source == UpdateSourceType::file)
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
                report.alreadyInstalledMessage = tr("The latest version is already installed");
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
    else
        report.version = contents.info.version.toString();

    if (!contents.cloudIsCompatible)
    {
        report.statusMessages << tr("Incompatible %1 instance. To update disconnect System from %1 first.",
            "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
            .arg(nx::branding::cloudName());
    }

    return report;
}

bool MultiServerUpdatesWidget::checkSpaceRequirements(const UpdateContents& contents) const
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

void MultiServerUpdatesWidget::openAdvancedSettings()
{
    const bool advancedMode = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    menu()->trigger(ui::action::AdvancedUpdateSettingsAction,
        ui::action::Parameters()
            .withArgument(Qn::AdvancedModeRole, advancedMode)
            .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>{this}));
}

void MultiServerUpdatesWidget::setUpdateTarget(
    const UpdateContents& contents, [[maybe_unused]] bool activeUpdate)
{
    NX_VERBOSE(this, "setUpdateTarget(%1)", contents.info.version);
    m_updateInfo = contents;
    m_updateSourceMode = contents.sourceType;

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
    m_stateTracker->setUpdateTarget(contents.info.version);
    m_forceUiStateUpdate = true;
    m_updateRemoteStateChanged = true;
    m_finishingForcefully = false;

    // TODO: We should collect all these changes to a separate state-structure.
    // TODO: We should split state flags more consistenly.
}

QnUuid MultiServerUpdatesWidget::clientPeerId() const
{
    return m_stateTracker->getClientPeerId(systemContext());
}

void MultiServerUpdatesWidget::setDayWarningVisible(bool visible)
{
    if (m_dayWarningVisible == visible)
        return;

    m_dayWarningVisible = visible;
    updateAlertBlock();
}

void MultiServerUpdatesWidget::updateAlertBlock()
{
    std::vector<BarDescription> messages;

    if (m_dayWarningVisible)
    {
        messages.push_back(
            {.text = tr("Applying System updates at the end of the week is not recommended"),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty = &messageBarSettings()->multiServerUpdateWeekendWarning});
    }

    if (nx::branding::isDesktopClientCustomized())
    {
        messages.push_back(
            {.text = tr(
                 "You are using a custom client. Please contact %1 to get the update instructions.")
                     .arg(nx::branding::company()),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty = &messageBarSettings()->multiServerUpdateCustomClientWarning});
    }

    ui->messageBarBlock->setMessageBars(messages);
}

nx::utils::Url MultiServerUpdatesWidget::generateUpcombinerUrl() const
{
    const common::update::PersistentUpdateStorage updateStorage =
        systemContext()->globalSettings()->targetPersistentUpdateStorage();

    const bool useLatest = m_updateInfo.sourceType == UpdateSourceType::internet;
    const nx::utils::SoftwareVersion targetVersion = useLatest
        ? nx::utils::SoftwareVersion()
        : m_updateInfo.info.version;

    QUrlQuery query;

    if (targetVersion.isNull())
    {
        // We are here if we have failed to check update in the Internet.
        // Changeset will contain requested build number in this case, or 'latest'.
        query.addQueryItem("version", "latest");
        query.addQueryItem("current", getMinimumComponentVersion().toString());
    }
    else
    {
        query.addQueryItem("version", m_updateInfo.info.version.toString());
        query.addQueryItem("password",
            common::update::passwordForBuild(QString::number(m_updateInfo.info.version.build)));
    }

    query.addQueryItem("customization", nx::branding::customization());

    QSet<nx::utils::OsInfo> osInfoList;
    for (const auto& id: m_stateTracker->allPeers())
    {
        const auto& server = resourcePool()->getResourceById<QnMediaServerResource>(id);
        if (!server)
            continue;

        if (!server->getOsInfo().isValid())
            continue;

        if (!targetVersion.isNull() && server->getVersion() == targetVersion)
            continue;

        osInfoList.insert(server->getOsInfo());
    }

    const bool includeAllClientPackages =
        updateStorage.autoSelection || !updateStorage.servers.isEmpty();

    query.addQueryItem("allClients", includeAllClientPackages ? "true" : "false");
    query.addQueryItem("components", getComponentQuery(osInfoList));

    nx::utils::Url url(common::update::updateGeneratorUrl());
    url.setQuery(query);

    return url;
}

nx::utils::SoftwareVersion MultiServerUpdatesWidget::getMinimumComponentVersion() const
{
    nx::utils::SoftwareVersion minVersion = appContext()->version();
    const auto allServers = resourcePool()->servers();
    for (const QnMediaServerResourcePtr& server: allServers)
    {
        if (server->getVersion() < minVersion)
            minVersion = server->getVersion();
    }
    return minVersion;
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
        if (checkResponse.sourceType == UpdateSourceType::mediaservers)
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
    m_updateInfo = {};
    m_updatesModel->setUpdateTarget({});
    m_stateTracker->clearVerificationErrors();
    m_forceUiStateUpdate = true;
    m_updateCheck = {};
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
    VersionSelectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_updateSourceMode = UpdateSourceType::internetSpecific;
    m_pickedSpecificBuild = dialog.version();
    recheckSpecificBuild();
}

void MultiServerUpdatesWidget::recheckSpecificBuild()
{
    if (m_updateSourceMode != UpdateSourceType::internetSpecific || !isVisible())
        return;

    m_forceUiStateUpdate = true;

    clearUpdateInfo();
    m_updateCheck = m_serverUpdateTool->checkForUpdate(
        update::CertainVersionParams{m_pickedSpecificBuild, appContext()->version()});
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
            std::promise<UpdateContents> promise;
            m_updateCheck = promise.get_future();
            promise.set_value(contents);
        }
    }

    if (!m_updateCheck.valid())
    {
        clearUpdateInfo();
        m_updateCheck = m_serverUpdateTool->checkForUpdate(
            update::LatestVmsVersionParams{appContext()->version()});
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
        int acceptedEula = appContext()->localSettings()->acceptedEulaVersion();
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
        if (m_clientUpdateTool->isVersionInstalled(m_updateInfo.info.version)
            || !m_updateInfo.needClientUpdate)
        {
            targets.remove(clientPeerId());
        }

        m_stateTracker->setUpdateTarget(m_updateInfo.info.version);
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
            m_serverUpdateTool->stopAllUploads();
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
            m_finishingForcefully = true;
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
    if (!NX_ASSERT(m_widgetState == WidgetUpdateState::finishingInstall))
        return;

    if (success)
    {
        m_stateTracker->processInstallTaskSet();
        const QSet<QnUuid> peersIssued = m_stateTracker->peersIssued();
        const QSet<QnUuid> peersComplete = m_stateTracker->peersComplete();
        const QSet<QnUuid> serversComplete = peersComplete - QSet{clientPeerId()};
        const QSet<QnUuid> peersFailed = m_stateTracker->peersFailed();

        if (m_finishingForcefully || !serversComplete.empty() || peersComplete == peersIssued)
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
            if (m_clientUpdateTool->shouldRestartTo(m_updateInfo.info.version))
            {
                QString appName = nx::branding::desktopClientDisplayName();
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
                && m_clientUpdateTool->shouldRestartTo(m_updateInfo.info.version);

            completeClientInstallation(shouldRestartClient);
        }
        // No servers have installed updates
        else if (!peersFailed.empty())
        {
            NX_ERROR(this, "atFinishUpdateComplete() - installation has failed completely");
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("There was an error while installing updates:"));

            PeerStateTracker::ErrorReport report;
            m_stateTracker->getErrorReport(report);

            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(report.peers));
            QString text = nx::vms::common::html::paragraph(report.message);
            text += nx::vms::common::html::paragraph(
                tr("If the problem persists, please contact Customer Support."));
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

    qnClientMessageProcessor->holdConnection(QnClientMessageProcessor::HoldConnectionPolicy::none);

    if (hasPendingUiChanges())
        loadDataToUi();
}

void MultiServerUpdatesWidget::repeatUpdateValidation()
{
    if (!m_updateInfo.isEmpty())
    {
        NX_INFO(this, "repeatUpdateValidation() - recalculating update report");
        auto installedVersions = m_clientUpdateTool->getInstalledClientVersions();
        if (m_updateInfo.error != common::update::InformationError::httpError
            && m_updateInfo.error != common::update::InformationError::networkError)
        {
            m_updateInfo.error = common::update::InformationError::noError;
        }

        m_serverUpdateTool->verifyUpdateManifest(m_updateInfo, installedVersions);
        setUpdateTarget(m_updateInfo, m_updateInfo.sourceType != UpdateSourceType::mediaservers);
    }
    else
    {
        NX_INFO(this,
            "repeatUpdateValidation() - update info is completely empty. Nothing to recalculate");
    }

    if (m_forceUiStateUpdate)
        loadDataToUi();
}

void MultiServerUpdatesWidget::atServerPackageDownloaded(const nx::vms::update::Package& package)
{
    NX_INFO(this, "Package download finished: \"%1\". Current state: %2",
        package.file, m_widgetState());

    if (m_widgetState == WidgetUpdateState::downloading)
    {
        m_serverUpdateTool->uploadPackageToRecipients(
            package, m_serverUpdateTool->getDownloadDir());
    }
}

void MultiServerUpdatesWidget::atServerPackageDownloadFailed(
    const nx::vms::update::Package& package,
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
            m_stateTracker->setTaskError(
                m_updateInfo.packageProperties[package.file].targets, "OfflineDownloadError");
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

    if (!m_stateTracker->findItemById(item->id))
    {
        NX_VERBOSE(this, "Peer %1 was removed. We should repeat validation.", item->id);
        repeatUpdateValidation();
    }
    else
    {
        if (!item->offline || m_widgetState == WidgetUpdateState::ready)
        {
            // TODO: this triggers too often, when server goes offline, online, added or removed.
            // We need to get these events here.
            NX_VERBOSE(this, "Server %1 has changed online status. We should repeat validation.",
                item->id);
            repeatUpdateValidation();
        }

        if (item->offline)
        {
            m_updateRemoteStateChanged = true;
            if (item->isServer())
                m_serverUpdateTool->stopUploadsToServer(item->id);
        }
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
        auto peersDownloadingOfflineUpdates = m_stateTracker->peersWithOfflinePackages();
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
                    case nx::vms::common::update::Status::Code::idle:
                    case nx::vms::common::update::Status::Code::starting:
                        // This stage has zero progress.
                        ++result.active;
                        break;
                    case nx::vms::common::update::Status::Code::downloading:
                        result.current += item->progress;
                        ++result.active;
                        break;
                    case nx::vms::common::update::Status::Code::error:
                    case nx::vms::common::update::Status::Code::preparing:
                    case nx::vms::common::update::Status::Code::readyToInstall:
                        result.current += 100;
                        ++result.done;
                        break;
                    case nx::vms::common::update::Status::Code::latestUpdateInstalled:
                        if (peersIssued.contains(id))
                        {
                            result.current += 100;
                            ++result.done;
                        }
                        break;
                    case nx::vms::common::update::Status::Code::offline:
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

        result.uploadingOfflinePackages = false;
        for (const QnUuid& serverId: peersDownloadingOfflineUpdates)
        {
            const UpdateItemPtr item = m_stateTracker->findItemById(serverId);
            const auto maxOfflinePackageProgress = item->offlinePackageCount * 100;
            result.max += maxOfflinePackageProgress;
            result.current += item->offlinePackageProgress;
            result.active += item->offlinePackageCount;

            result.uploadingOfflinePackages |=
                (item->offlinePackageProgress < maxOfflinePackageProgress);
        }

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
        m_updateCheck = m_serverUpdateTool->checkForUpdate(
            update::LatestVmsVersionParams{appContext()->version()});
    }

    if (!m_serverUpdateCheck.valid())
        m_serverUpdateCheck = m_serverUpdateTool->checkMediaserverUpdateInfo();

    if (!m_serverStatusCheck.valid())
        m_serverStatusCheck = m_serverUpdateTool->requestRemoteUpdateState();

    m_offlineUpdateCheck = m_serverUpdateTool->takeUpdateCheckFromFile();
    if (!m_offlineUpdateCheck.valid())
    {
        NX_VERBOSE(this, "processInitialState() - there was no offline update file. State=%1",
            toString(m_serverUpdateTool->getUploaderState()));
    }

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

    if (mediaserverUpdateCheckReady
        && mediaserverStatusCheckReady
        && offlineCheckReady)
    {
        // TODO: We should invent a method for testing this.
        auto updateInfo = m_serverUpdateCheck.get();
        auto serverStatus = m_serverStatusCheck.get();
        bool haveDownloadsInProgress = false;

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
        else
        {
            NX_DEBUG(this, "processInitialCheckState() - there was no offline update check.");
        }

        for (const auto& [id, status]: serverStatus)
        {
            for (const common::update::PackageDownloadStatus& packageStatus: status.downloads)
            {
                if (packageStatus.status
                    != common::update::PackageDownloadStatus::Status::downloaded)
                {
                    haveDownloadsInProgress = true;
                    break;
                }
            }

            if (haveDownloadsInProgress)
                break;
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

        // if update process is already in 'installing' phase.
        if (m_updateInfo.preferOtherUpdate(updateInfo))
        {
            NX_INFO(this, "processInitialCheckState() - taking other update info from \"%1\"",
                toString(updateInfo.sourceType));
            setUpdateTarget(updateInfo, /*activeUpdate=*/true);
            // TODO: It can be update from file
            m_clientUpdateTool->setUpdateTarget(updateInfo);
        }

        auto serversHaveDownloaded = m_stateTracker->peersInState(
            StatusCode::readyToInstall, /*withClients=*/false);
        auto serversAreDownloading = m_stateTracker->peersInState(
            StatusCode::downloading, /*withClients=*/false);
        auto serversWithError = m_stateTracker->peersInState(
            StatusCode::error, /*withClients=*/false);
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
        else if (!serversAreDownloading.empty() || !serversWithDownloadingError.empty()
            || haveDownloadsInProgress)
        {
            auto targets = serversAreDownloading + serversWithDownloadingError;
            NX_INFO(this,
                "processInitialCheckState() - servers %1 are in downloading or error state",
                targets);

            setTargetState(WidgetUpdateState::downloading, targets);
            // TODO: We should check whether we have initiated an update. Maybe we should
            // not start manual uploads.
            if (!m_updateInfo.manualPackages.empty() && m_updateInfo.noServerWithInternet)
                m_serverUpdateTool->startManualDownloads(m_updateInfo);

            const auto uploaderState = m_serverUpdateTool->getUploaderState();
            if (uploaderState == ServerUpdateTool::OfflineUpdateState::push
                || uploaderState == ServerUpdateTool::OfflineUpdateState::ready)
            {
                m_serverUpdateTool->startUpload(m_updateInfo, /*cleanExisting*/ true);
            }
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
    auto peersWithOfflinePackages = m_stateTracker->peersWithOfflinePackages();
    auto completedPeersWithOfflinePackages =
        m_stateTracker->peersCompletedOfflinePackagesDownload();
    auto failedPeersWithOfflinePackages =
        m_stateTracker->peersWithOfflinePackageDownloadError();

    NX_VERBOSE(this,
        "processDownloadingState(): "
            "active: %1, unknown: %2, failed: %3, complete: %4, issued: %5, "
            "with offline packages: %6, with offline packages completed: %7, "
            "with offline packages failed: %8",
        peersActive.size(),
        peersUnknown.size(),
        peersFailed.size(),
        peersComplete.size(),
        peersIssued.size(),
        peersWithOfflinePackages.size(),
        completedPeersWithOfflinePackages.size(),
        failedPeersWithOfflinePackages.size());

    NX_VERBOSE(this, "processDownloadingState(): issued: %1", peersIssued);

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
            m_serverUpdateTool->startUpload(m_updateInfo, /*cleanExisting=*/true);
        }
        else
        {
            NX_ERROR(this, "processDownloadingState() - no servers downloading or an error.");
        }
    }

    processUploaderChanges();

    const bool fileTransfersNotDone = peersActive.size() + peersUnknown.size() > 0
        || completedPeersWithOfflinePackages.size() < peersWithOfflinePackages.size();

    if (fileTransfersNotDone && peersFailed.empty() && failedPeersWithOfflinePackages.empty())
        return;

    // No peers are doing anything. So we consider current state transition is complete
    NX_INFO(this) << "processDownloadingState() - download has stopped";

    if (peersComplete.size() >= peersIssued.size() && failedPeersWithOfflinePackages.empty())
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
        text += nx::vms::common::html::paragraph(
            tr("If the problem persists, please contact Customer Support."));
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
            m_stateTracker->resetOfflinePackagesInformation(failedPeersWithOfflinePackages);
            if (!failedPeersWithOfflinePackages.isEmpty()
                || !m_updateInfo.manualPackages.isEmpty())
            {
                m_serverUpdateTool->startUpload(
                    m_updateInfo, /*cleanExisting*/ true, /*force*/ true);
            }
        }
        else if (clicked == cancelUpdate)
        {
            setTargetState(WidgetUpdateState::cancelingDownload, {});
            m_serverUpdateTool->requestStopAction();
        }
    }
}

void MultiServerUpdatesWidget::processReadyInstallState()
{
    m_stateTracker->processReadyInstallTaskSet();

    auto idle = m_stateTracker->peersInState(StatusCode::idle);
    auto starting = m_stateTracker->peersInState(StatusCode::starting);
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
        setTargetState(WidgetUpdateState::downloading, downloading + starting, false);
    }
    else if (!starting.isEmpty())
    {
        NX_DEBUG(this, "processReadyInstallState() - detected servers %1 in starting state",
            starting);
        setTargetState(WidgetUpdateState::downloading, starting, false);
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
    std::optional<nx::vms::client::core::LogonData> logonData;
    if (const auto connection = this->connection(); NX_ASSERT(connection))
        logonData = connection->createLogonData();

    // Client must be forcefully disconnected before restarting to make sure all components
    // are deinitialized correctly.
    qnClientMessageProcessor->holdConnection(QnClientMessageProcessor::HoldConnectionPolicy::none);

    const QnUuidSet incompatibleServers = m_stateTracker->serversWithChangedProtocol();
    if (clientUpdated || !incompatibleServers.empty())
    {
        NX_INFO(this, "completeInstallation() - servers %1 have new protocol. Forcing reconnect",
            incompatibleServers);

        menu()->trigger(action::DisconnectAction, {Qn::ForceRole, true});
    }

    if (clientUpdated)
    {
        NX_INFO(this, "completeInstallation() - restarting the client");
        if (m_clientUpdateTool->restartClient(logonData))
            return;

        NX_ERROR(this, "completeInstallation(%1) - failed to run restart command",
            clientUpdated);
        QnConnectionDiagnosticsHelper::failedRestartClientMessage(this);
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

    if (state == ServerUpdateTool::OfflineUpdateState::push
        || state == ServerUpdateTool::OfflineUpdateState::ready
        || state == ServerUpdateTool::OfflineUpdateState::done)
    {
        // Should check all downloading servers and resume uploads if there are not any.
        auto serversDownloading = m_stateTracker->peersInState(StatusCode::downloading,
            /*withClient*/false);
        for (const auto id: serversDownloading)
        {
            bool hasUploads = m_serverUpdateTool->hasActiveUploadsTo(id);
            bool hasUpdate = m_updateInfo.peersWithUpdate.contains(id);
            if (!hasUploads && hasUpdate)
            {
                NX_DEBUG(this, "processUploaderChanges() - starting uploads to server %1", id);
                m_serverUpdateTool->startUploadsToServer(m_updateInfo, id);
            }
        }
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
                    auto manager = context()->instance<workbench::LocalNotificationsManager>();
                    // TODO: We should show 'Pushing updates...'
                    m_rightPanelDownloadProgress =
                        manager->addProgress(tr("Downloading updates..."));
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
                    {
                        qnClientMessageProcessor->holdConnection(
                            QnClientMessageProcessor::HoldConnectionPolicy::update);
                    }
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
    auto manager = context()->instance<workbench::LocalNotificationsManager>();
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

    ui->errorLabel->setText(report.statusMessages.join(nx::vms::common::html::kLineBreak));
    if (report.version.isEmpty())
        ui->targetVersionLabel->setText(kNoVersionNumberText);
    else
        ui->targetVersionLabel->setText(report.version);

    setHighlightMode(ui->targetVersionLabel, report.versionHighlight);
    setHighlightMode(ui->errorLabel, report.statusHighlight);

    ui->latestVersionBannerLabel->setText(m_updateReport->alreadyInstalledMessage);
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
    bool hasLatestVersion = m_updateReport->hasLatestVersion;

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
        m_updatesModel->setUpdateTarget(m_updateInfo.info.version);
    }

    ui->selectUpdateTypeButton->setText(toString(m_updateSourceMode));

    const bool showDownloadButton = !common::update::updateGeneratorUrl().isEmpty()
        && !isChecking && !latestVersion
        && m_updateSourceMode != UpdateSourceType::file
        && (m_widgetState == WidgetUpdateState::ready
            || m_widgetState != WidgetUpdateState::initial)
        && (m_updateInfo.error == common::update::InformationError::networkError
            // If one wants to download a file in another place.
            || m_updateInfo.error == common::update::InformationError::noError)
        && m_widgetState != WidgetUpdateState::readyInstall
        // httpError corresponds to 'Build not found'
        && m_updateInfo.error != common::update::InformationError::httpError;

    ui->manualDownloadButton->setVisible(showDownloadButton || ini().alwaysShowGetUpdateFileButton);

    const bool showCheckAgainButton = !isChecking
        && m_updateSourceMode != UpdateSourceType::file
        && (m_widgetState == WidgetUpdateState::ready
            || m_widgetState == WidgetUpdateState::initial)
        && m_updateInfo.info.version.isNull();
    ui->checkAgainButton->setVisible(showCheckAgainButton);

    const bool showTryAgainButton =
        showDownloadButton && m_widgetState != WidgetUpdateState::ready;
    ui->tryAgainButton->setVisible(showTryAgainButton);

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
    if (info.downloadingServers)
        caption = tr("Downloading updates...");
    else if (info.downloadingClient)
        caption = tr("Downloading client package...");
    else if (info.uploadingOfflinePackages)
        caption = tr("Uploading offline update packages to Servers...");
    else if (info.installingServers)
        caption = tr("Installing updates...");
    else if (info.installingClient)
        caption = tr("Installing client updates...");

    ui->actionProgess->setTextVisible(!caption.isEmpty());
    if (!caption.isEmpty())
        ui->actionProgess->setFormat(caption + "\t");

    if (!m_rightPanelDownloadProgress.isNull())
    {
        auto manager = context()->instance<workbench::LocalNotificationsManager>();
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
        ui->spaceErrorLabel->setText(tr("There is not enough space on your computer to download "
            "the Client update. Please free up some space on your hard drive and try again."));
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
        ? QString("text_buttons/arrow_down_20.svg")
        : QString("text_buttons/arrow_up_20.svg");
    ui->advancedUpdateSettings->setIcon(qnSkin->icon(icon, kIconSubstitutions));
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
    setDayWarningVisible(endOfTheWeek);

    updateAlertBlock();

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
        QStringList debugState = {
            nx::format("ReleaseListUrl=<a href=\"%1\">%1</a>",
                common::update::releaseListUrl(systemContext())),
            nx::format("UpdateGeneratorUrl=<a href=\"%1\">%1</a>",
                common::update::updateGeneratorUrl()),
            nx::format("Widget=%1", toString(m_widgetState)),
            nx::format("Widget source=%1, Update source=%2",
                toString(m_updateSourceMode), toString(m_updateInfo.sourceType)),
            nx::format("UploadTool=%1", m_serverUpdateTool->getUploaderState()),
            nx::format("ClientTool=%1",
                ClientUpdateTool::toString(m_clientUpdateTool->getState())),
            nx::format("validUpdate=%1", m_updateInfo.isValidToInstall()),
            nx::format("targetVersion=%1", m_updateInfo.info.version),
            nx::format("checkUpdate=%1", m_updateCheck.valid()),
            nx::format("checkServerUpdate=%1", m_serverUpdateCheck.valid()),
            nx::format("<a href=\"%1\">/ec2/updateStatus</a>",
                m_serverUpdateTool->getUpdateStateUrl()),
            nx::format("<a href=\"%1\">/ec2/updateInformation</a>",
                m_serverUpdateTool->getUpdateInformationUrl()),
            nx::format("<a href=\"%1\">/ec2/updateInformation?version=installed</a>",
                m_serverUpdateTool->getInstalledUpdateInfomationUrl()),
            nx::format("lowestVersion=%1", m_stateTracker->lowestInstalledVersion()),
        };

        toDebugString(debugState, "installing", m_serverUpdateTool->getServersInstalling());
        toDebugString(debugState, "issued", m_stateTracker->peersIssued());
        toDebugString(debugState, "failed", m_stateTracker->peersFailed());
        toDebugString(debugState, "with update", m_updateInfo.peersWithUpdate);
        QSet<QnUuid> activeServers;
        for (const auto& server: m_stateTracker->activeServers())
            activeServers.insert(server.first);
        toDebugString(debugState, "activeServers", activeServers);

        if (m_updateInfo.error != common::update::InformationError::noError)
            debugState << nx::format("updateInfoError=%1", m_updateInfo.error);

        if (stateHasProgress(m_widgetState))
        {
            ServerUpdateTool::ProgressInfo info = calculateActionProgress();
            debugState << nx::format("progress=%1 of %2, active=%3, done=%4",
                info.current, info.max, info.active, info.done);
        }

        if (m_widgetState == WidgetUpdateState::installing
            || m_widgetState == WidgetUpdateState::installingStalled)
        {
            auto installDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                m_serverUpdateTool->getInstallDuration());
            debugState << nx::format("duration=%1", installDuration.count());
        }

        if (m_updateInfo.isValidToInstall() && m_updateInfo.noServerWithInternet)
            debugState << "noServerWithInternet";

        QString debugText = debugState.join(nx::vms::common::html::kLineBreak);
        if (debugText != ui->debugStateLabel->text())
            ui->debugStateLabel->setText(debugText);
    }

    ui->debugStateLabel->setVisible(m_showDebugData);
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
        case ServerUpdateTool::OfflineUpdateState::push:
            return "push";
        case ServerUpdateTool::OfflineUpdateState::done:
            return "done";
        case ServerUpdateTool::OfflineUpdateState::error:
            return "error";
    }
    return "Unknown update source mode";
}

QString MultiServerUpdatesWidget::toString(UpdateSourceType mode)
{
    switch (mode)
    {
        case UpdateSourceType::internet:
            return tr("Latest Available Update");
        case UpdateSourceType::internetSpecific:
            return tr("Specific Build");
        case UpdateSourceType::file:
            return tr("Browse for Update File");
        case UpdateSourceType::mediaservers:
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
