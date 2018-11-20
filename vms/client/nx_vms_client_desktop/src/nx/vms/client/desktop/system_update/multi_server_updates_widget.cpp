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
#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench_context.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/models/resource/resource_list_model.h>
#include <update/low_free_space_warning.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/workbench/extensions/workbench_progress_manager.h>
#include <nx/network/app_info.h>

#include <nx/vms/client/desktop/ini.h>
#include <utils/common/html.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/applauncher_utils.h>

#include "server_update_tool.h"
#include "server_updates_model.h"
#include "server_status_delegate.h"

using namespace nx::vms::client::desktop::ui;

namespace {

const int kLongInstallWarningTimeoutMs = 2 * 60 * 1000; // 2 minutes
const int kTooLateDayOfWeek = Qt::Thursday;
const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour
const int kVersionLabelFontSizePixels = 24;
const int kVersionLabelFontWeight = QFont::DemiBold;

// Height limit for servers list in dialog box with update report
static constexpr int kSectionHeight = 150;
static constexpr int kSectionMinHeight = 30;

constexpr auto kLatestVersionBannerLabelFontSizePixels = 22;
constexpr auto kLatestVersionBannerLabelFontWeight = QFont::Light;

const auto kWaitForUpdateCheck = std::chrono::milliseconds(10);

const int kLinkCopiedMessageTimeoutMs = 2000;

// Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
const quint32 kProcessTerminateTimeoutMs = 15000;

/* N-dash 5 times: */
const QString kNoVersionNumberText = QString::fromWCharArray(L"\x2013\x2013\x2013\x2013\x2013");

QString versionText(const nx::utils::SoftwareVersion& version)
{
    return version.isNull() ? kNoVersionNumberText : version.toString();
}

// Adds resource list to message box
void injectResourceList(QnSessionAwareMessageBox& messageBox, const QnResourceList& resources)
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

    auto debugFlags = Ini::UpdateDebugFlags(ini().massSystemUpdateDebugInfo);
    m_showDebugData = debugFlags.testFlag(Ini::UpdateDebugFlag::showInfo);

    m_serverUpdateTool.reset(new ServerUpdateTool(this));
    m_clientUpdateTool.reset(new ClientUpdateTool(this));

    m_updatesModel = m_serverUpdateTool->getModel();

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

    m_sortedModel.reset(new QnSortedServerUpdatesModel(this));
    m_sortedModel->setSourceModel(m_updatesModel.get());
    ui->tableView->setModel(m_sortedModel.get());
    m_statusItemDelegate.reset(new ServerStatusItemDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(ServerUpdatesModel::ProgressColumn, m_statusItemDelegate.get());

    connect(m_sortedModel.get(), &QnSortedServerUpdatesModel::dataChanged,
        this, &MultiServerUpdatesWidget::atModelDataChanged);

    m_updatesModel->setResourceFeed(resourcePool());
    m_serverUpdateTool->setResourceFeed(resourcePool());

    // the column does not matter because the model uses column-independent sorting
    m_sortedModel->sort(0);

    if (auto horizontalHeader = ui->tableView->horizontalHeader())
    {
        horizontalHeader->setDefaultAlignment(Qt::AlignLeft);
        horizontalHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::StatusMessageColumn,
            QHeaderView::Stretch);
        horizontalHeader->setSectionsClickable(false);
    }

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
    /*
    connect(ui->autoCheckUpdates, &QCheckBox::stateChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);
        */

    connect(ui->releaseNotesLabel, &QLabel::linkActivated, this,
        [this]()
        {
            if (m_haveValidUpdate && !m_updateInfo.info.releaseNotesUrl.isEmpty())
                QDesktopServices::openUrl(m_updateInfo.info.releaseNotesUrl);
        });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
        this, &MultiServerUpdatesWidget::checkForInternetUpdates);

    setWarningStyle(ui->errorLabel);
    setWarningStyle(ui->longUpdateWarning);

    ui->infoStackedWidget->setCurrentWidget(ui->errorPage);
    ui->errorLabel->setText(QString());
    setWarningStyle(ui->errorLabel);

    ui->dayWarningBanner->setBackgroundRole(QPalette::AlternateBase);
    ui->dayWarningLabel->setForegroundRole(QPalette::Text);

    static_assert(kTooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningBanner->setVisible(false);
    ui->longUpdateWarning->setVisible(false);
    ui->browseUpdate->setVisible(false);

    ui->releaseNotesLabel->setText(QString("<a href='notes'>%1</a>").arg(tr("Release notes")));
    ui->releaseDescriptionLabel->setText(QString());
    ui->releaseDescriptionLabel->setVisible(false);

    QTimer* updateTimer = new QTimer(this);
    updateTimer->setSingleShot(false);
    updateTimer->start(kAutoCheckIntervalMs);
    connect(updateTimer, &QTimer::timeout,
        this, &MultiServerUpdatesWidget::autoCheckForUpdates);

    m_longUpdateWarningTimer.reset(new QTimer(this));
    m_longUpdateWarningTimer->setInterval(kLongInstallWarningTimeoutMs);
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

    m_updateCheck = m_serverUpdateTool->getUpdateCheck();

    const auto connection = commonModule()->ec2Connection();
    if (connection)
    {
        QnConnectionInfo connectionInfo = connection->connectionInfo();

        QnUuid serverId = QnUuid(connectionInfo.ecsGuid);
        nx::utils::Url serverUrl = connectionInfo.ecUrl;
        m_clientUpdateTool->setServerUrl(serverUrl, serverId);
        m_clientUpdateTool->requestRemoteUpdateInfo();
    }
    // Force update when we open dialog.
    checkForInternetUpdates();

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

    m_autoCheckUpdate = mode;
}

void MultiServerUpdatesWidget::initDropdownActions()
{
    m_selectUpdateTypeMenu.reset(new QMenu(this));
    m_selectUpdateTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceType::internet),
        [this]()
        {
            setUpdateSourceMode(UpdateSourceType::internet);
        });

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceType::internetSpecific),
        [this]()
        {
            setUpdateSourceMode(UpdateSourceType::internetSpecific);
        });

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceType::file),
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

    setAutoUpdateCheckMode(true);
}

void MultiServerUpdatesWidget::initDownloadActions()
{
    auto downloadLinkMenu = new QMenu(this);
    downloadLinkMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    downloadLinkMenu->addAction(tr("Download in External Browser"),
        [this]()
        {
            QSet<QnUuid> targets = m_serverUpdateTool->getAllServers();
            auto url = generateUpdatePackageUrl(m_updateInfo, targets, resourcePool()).toString();

            QDesktopServices::openUrl(url);
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            QSet<QnUuid> targets = m_serverUpdateTool->getAllServers();
            auto url = generateUpdatePackageUrl(m_updateInfo, targets, resourcePool()).toString();
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

void MultiServerUpdatesWidget::atUpdateCurrentState()
{
    NX_ASSERT(m_serverUpdateTool);

    if (isHidden())
        return;

    // We poll all our tools for new information. Then we update UI if there are any changes
    if (m_updateCheck.valid() && m_updateCheck.wait_for(kWaitForUpdateCheck) == std::future_status::ready)
    {
        auto checkResponse = m_updateCheck.get();
        NX_VERBOSE(this) << "atUpdateCurrentState got update info:"
            << checkResponse.info.version
            << "from" << checkResponse.source;

        m_serverUpdateTool->verifyUpdateManifest(checkResponse);
        m_updateLocalStateChanged = true;
        m_updateInfo = checkResponse;
        m_haveValidUpdate = false;
        m_updateCheckError = "";
        m_targetVersion = nx::utils::SoftwareVersion(checkResponse.info.version);
        m_targetChangeset = m_targetVersion.build();

        using Error = nx::update::InformationError;
        if (checkResponse.isValid())
        {
            m_haveValidUpdate = true;
        }
        else
        {
            QString internalError = nx::update::toString(checkResponse.error);
            NX_WARNING(this) << "atUpdateCurrentState(" << m_updateInfo.info.version << ") detected a problem" << internalError;
            switch(checkResponse.error)
            {
                case Error::noError:
                    // Everything is good. Not should be here.
                    NX_ASSERT(false);
                    break;
                case Error::networkError:
                    // Unable to check update from the internet.
                    m_updateCheckError = tr("Unable to check updates on the internet");
                    break;
                case Error::httpError:
                    m_updateCheckError = tr("Unable to check updates on the internet");
                    break;
                case Error::jsonError:
                    m_updateCheckError = tr("Invalid update information");
                    break;
                case Error::incompatibleVersion:
                    m_updateCheckError = tr("Incompatible update version");
                    break;
                case Error::incompatibleCloudHostError:
                    m_updateCheckError = tr("Incompatible %1 instance. To update disconnect System from %1 first.",
                        "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
                        .arg(nx::network::AppInfo::cloudName());
                    break;
                case Error::notFoundError:
                    // No update
                    break;
                case Error::noNewVersion:
                    // We have most recent version for this build.
                    break;
                default:
                    m_updateCheckError = nx::update::toString(checkResponse.error);
                    break;
            }

            auto info = QnAppInfo::currentSystemInformation();
            auto currentVersion = nx::utils::SoftwareVersion(info.version);

            if (m_haveValidUpdate && m_updateInfo.getVersion() < currentVersion)
            {
                m_updateCheckError = tr("Downgrade to earlier versions is not possible");
            }
            else if (!m_updateInfo.invalidVersion.empty())
            {
                m_updateCheckError = tr("This version does not include some necessary Server package");
            }
        }

        m_updateLocalStateChanged = true;
    }

    // Maybe we should not call it right here.
    m_serverUpdateTool->requestRemoteUpdateState();

    processRemoteChanges();
    processUploaderChanges();

    if (m_serverUpdateTool->hasRemoteChanges() || m_updateLocalStateChanged)
        loadDataToUi();
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
    m_updateInfo = nx::update::UpdateContents();
    m_updatesModel->setUpdateTarget(nx::utils::SoftwareVersion());
    m_updateLocalStateChanged = true;
    m_updateCheck = std::future<nx::update::UpdateContents>();
}

void MultiServerUpdatesWidget::pickLocalFile()
{
    auto options = QnCustomFileDialog::fileDialogOptions();
    QString caption = tr("Select Update File...");
    QString filter = tr("Update Files") + " (*.zip)";
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
    nx::utils::SoftwareVersion version = qnStaticCommon->engineVersion();
    auto buildNumber = dialog.buildNumber();

    m_targetVersion = nx::utils::SoftwareVersion(version.major(), version.minor(), version.bugfix(), buildNumber);
    m_targetChangeset = dialog.changeset();
    QString updateUrl = qnSettings->updateFeedUrl();
    m_updateCheck = nx::update::checkSpecificChangeset(updateUrl, dialog.changeset());
    loadDataToUi();
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

    if (m_updateStateCurrent == WidgetUpdateState::readyInstall)
    {
        auto targets = m_serverUpdateTool->getServersInState(StatusCode::readyToInstall);
        if (targets.empty())
        {
            NX_VERBOSE(this) << "atStartUpdateAction() - no server can install anything";
            return;
        }

        NX_VERBOSE(this)
            << "atStartUpdateAction() - starting installation for"
            << targets.size() << ":" << targets;
        setTargetState(WidgetUpdateState::installing, targets);
    }
    else if (m_updateStateCurrent == WidgetUpdateState::ready && m_haveValidUpdate)
    {
        int acceptedEula = qnSettings->acceptedEulaVersion();
        int newEula = m_updateInfo.info.eulaVersion;
        const bool showEula =  acceptedEula < newEula;

        if (showEula && !context()->showEulaMessage(m_updateInfo.eulaPath))
        {
            return;
        }

        if (m_updateSourceMode == UpdateSourceType::file)
        {
            setTargetState(WidgetUpdateState::pushing, {});
            m_serverUpdateTool->requestStartUpdate(m_updateInfo.info);
            m_clientUpdateTool->downloadUpdate(m_updateInfo);
            m_serverUpdateTool->startUpload(m_updateInfo);
        }
        else
        {
            // Start download process.
            // Move to state 'Downloading'
            // At this state should check remote state untill everything is complete.

            /*
            auto targets = m_updatesModel->getServersInState(StatusCode::available);
            if (targets.empty())
            {
                NX_VERBOSE(this) << "atStartUpdateAction() - no servers can download update";
                return;
            }*/
            auto targets = m_serverUpdateTool->getAllServers();
            NX_VERBOSE(this) << "atStartUpdateAction() - sending 'download' command";
            auto offlineServers = m_serverUpdateTool->getOfflineServers();
            if (!offlineServers.empty())
            {
                QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
                messageBox->setIcon(QnMessageBoxIcon::Warning);
                messageBox->setText(tr("Some servers are offline and will not be updated. Skip them?"));
                injectResourceList(*messageBox, resourcePool()->getResourcesByIds(offlineServers));
                messageBox->addCustomButton(QnMessageBoxCustomButton::Skip,
                    QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);
                auto cancel = messageBox->addButton(QDialogButtonBox::Cancel);

                messageBox->exec();
                auto clicked = messageBox->clickedButton();
                if (clicked == cancel)
                    return;
                targets.subtract(offlineServers);
            }

            auto incompatible = m_serverUpdateTool->getLegacyServers();
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

            setTargetState(WidgetUpdateState::downloading, targets);
            m_serverUpdateTool->requestStartUpdate(m_updateInfo.info);
            m_clientUpdateTool->downloadUpdate(m_updateInfo);
        }
    }
    else
    {
        NX_VERBOSE(this) << "atStartUpdateAction() - invalid widget state for download command";
    }

    if (m_updateRemoteStateChanged)
        loadDataToUi();
}

bool MultiServerUpdatesWidget::atCancelCurrentAction()
{
    closePanelNotifications();

    // Cancel all the downloading.
    if (m_updateStateCurrent == WidgetUpdateState::downloading)
    {
        NX_VERBOSE(this) << "atCancelCurrentAction() at" << toString(m_updateStateCurrent);
        auto serversToCancel = m_serversIssued;
        m_serverUpdateTool->requestStopAction();
        m_clientUpdateTool->resetState();
        setTargetState(WidgetUpdateState::initial, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::installing)
    {
        NX_VERBOSE(this) << "atCancelCurrentAction() at" << toString(m_updateStateCurrent);
        // Should send 'cancel' command to all the servers?
        auto serversToCancel = m_serverUpdateTool->getServersInstalling();
        m_serverUpdateTool->requestStopAction();
        m_clientUpdateTool->resetState();
        setTargetState(WidgetUpdateState::initial, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::readyInstall)
    {
        NX_VERBOSE(this) << "atCancelCurrentAction() at" << toString(m_updateStateCurrent);
        auto serversToCancel = m_serverUpdateTool->getServersInState(StatusCode::readyToInstall);
        m_serverUpdateTool->requestStopAction();
        m_clientUpdateTool->resetState();
        setTargetState(WidgetUpdateState::initial, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::pushing)
    {
        setTargetState(WidgetUpdateState::ready, {});
        m_serverUpdateTool->stopUpload();
        m_serverUpdateTool->requestStopAction();
    }
    else
    {
        NX_VERBOSE(this) << "atCancelCurrentAction() at" << toString(m_updateStateCurrent) << ": not implemented";
        return false;
    }

    clearUpdateInfo();
    setUpdateSourceMode(UpdateSourceType::internet);
    checkForInternetUpdates();

    if (m_updateRemoteStateChanged)
        loadDataToUi();

    // Spec says that we can not cancel anything when we began installing stuff.
    return true;
}

ServerUpdateTool::ProgressInfo MultiServerUpdatesWidget::calculateActionProgress() const
{
    if (m_updateStateCurrent == WidgetUpdateState::pushing)
    {
        return m_serverUpdateTool->calculateUploadProgress();
    }
    else if (m_updateStateCurrent == WidgetUpdateState::downloading)
    {
        ServerUpdateTool::ProgressInfo result;
        for (auto id : m_serversActive)
        {
            auto item = m_updatesModel->findItemById(id);
            if (!item)
                continue;

            result.current += item->progress;
            result.max += 100;
        }
        result.downloadingServers = !m_serversActive.empty();

        if (m_clientUpdateTool->hasUpdate())
        {
            result.current += m_clientUpdateTool->getDownloadProgress();
            result.max += 100;
            result.downloadingClient = !m_clientUpdateTool->isDownloadComplete();
        }
        return result;
    }
    else if (m_updateStateCurrent == WidgetUpdateState::installing)
    {
        ServerUpdateTool::ProgressInfo result;
        auto installing = m_serverUpdateTool->getServersInstalling();
        auto installed = m_serverUpdateTool->getServersCompleteInstall();
        // TODO: Deal with it.
        result.installingServers = !m_serversActive.empty();

        int total = installing.size() + installed.size();

        result.current += installed.size()*100;
        result.max += 100*total;

        if (m_clientUpdateTool->hasUpdate())
        {
            bool complete = m_clientUpdateTool->getState() == ClientUpdateTool::State::complete;
            if (complete)
                result.current += 100;
            result.installingClient = !complete;
            result.max += 100;
        }
        return result;
    }

    return ServerUpdateTool::ProgressInfo();
}

void MultiServerUpdatesWidget::processRemoteInitialState()
{
    if (!isVisible())
        return;
    auto downloaded = m_serverUpdateTool->getServersInState(StatusCode::readyToInstall);
    auto downloading = m_serverUpdateTool->getServersInState(StatusCode::downloading);
    auto installing = m_serverUpdateTool->getServersInstalling();
    auto installed = m_serverUpdateTool->getServersCompleteInstall();

    if (m_serverUpdateTool->haveActiveUpdate())
    {
        auto updateInfo = m_serverUpdateTool->getRemoteUpdateContents();
        NX_VERBOSE(this)
            << "processRemoteInitialState() - we have an active update process to version"
            << updateInfo.info.version;

        /*
         * TODO: We should deal with starting client update.
         * There are two distinct situations:
         *  1. This is the client, that has initiated update process.
         *  2. This is 'other' client, that have found that an update process is running.
         *  It should download an update package using p2p downloader
         */
        if (updateInfo.isValid())
        {
            m_updateCheck = std::future<nx::update::UpdateContents>();
            m_updateInfo = updateInfo;
            m_haveValidUpdate = true;
            m_clientUpdateTool->downloadUpdate(updateInfo);
        }
        else
        {
            m_updateCheck = m_serverUpdateTool->checkRemoteUpdateInfo();
            m_haveValidUpdate = false;
        }

        m_updateLocalStateChanged = true;

        if (!downloading.empty())
        {
            NX_VERBOSE(this)
                << "processRemoteInitialState() - some servers are downloading an update";
            setTargetState(WidgetUpdateState::downloading, downloading);
        }
        else if (!installing.empty())
        {
            NX_VERBOSE(this)
                << "processRemoteInitialState() - some servers are installing an update";
            setTargetState(WidgetUpdateState::installing, installing);
        }
        else if (!downloaded.empty())
        {
            NX_VERBOSE(this)
                << "processRemoteInitialState() - some servers have already downloaded an update";
            setTargetState(WidgetUpdateState::readyInstall, {});
        }
        else if (!installed.empty())
        {
            // We can be here when we have all servers installed all updates.
            // There will be some information in /ec2/updateInformation,
            // but all the servers from /ec2/updateStatus will tell 'No update information'.
            // Their version will be equal to the version in /ec2/updateInformation
            NX_VERBOSE(this)
                << "processRemoteInitialState() - servers" << installed
                << "have already downloaded an update";
            // Should check if there are any servers not completed update
            // Client could be there as well.
            setTargetState(WidgetUpdateState::readyInstall, {});
        }
        else
        {
            // We can reach here when we reconnect to the server with complete updates.
            NX_VERBOSE(this)
                << "processRemoteInitialState() - no servers in downloading/installing/downloaded/installed state."
                << "Update process seems to be stalled or complete. Ignoring this internal state.";
            setTargetState(WidgetUpdateState::ready, {});
        }

        loadDataToUi();
    }
    else
    {
        NX_VERBOSE(this)
            << "processRemoteInitialState() - we are in initial state and finally got status for remote system";
        setTargetState(WidgetUpdateState::ready, {});
    }
}

void MultiServerUpdatesWidget::processRemoteDownloading()
{
    auto allStates = m_serverUpdateTool->getAllServerStates();

    for (auto record: allStates)
    {
        StatusCode state = record.second;
        auto id = record.first;
        switch (state)
        {
            case StatusCode::readyToInstall:
                if (m_serversActive.contains(id))
                {
                    NX_VERBOSE(this)
                        << "processRemoteDownloading() - server "
                        << id << "completed downloading and is ready to install";
                    m_serversComplete.insert(id);
                    m_serversActive.remove(id);
                }
                break;
            case StatusCode::error:
            case StatusCode::idle:
                if (m_serversActive.contains(id))
                {
                    NX_VERBOSE(this)
                        << "processRemoteDownloading() - server "
                        << id << "failed to download update package";
                    m_serversFailed.insert(id);
                    m_serversActive.remove(id);
                }
                break;
            case StatusCode::preparing:
            case StatusCode::downloading:
                if (!m_serversActive.contains(id) && m_serversIssued.contains(id))
                {
                    NX_VERBOSE(this)
                        << "processRemoteDownloading() - server "
                        << id << "resumed downloading.";
                    m_serversActive.remove(id);
                }
                break;
            case StatusCode::latestUpdateInstalled:
                if (m_serversActive.contains(id))
                {
                    NX_VERBOSE(this)
                        << "processRemoteDownloading() - server "
                        << id << "have already installed this package.";
                    m_serversActive.remove(id);
                }
                break;
            case StatusCode::offline:
                if (m_serversActive.contains(id))
                {
                    NX_VERBOSE(this)
                        << "processRemoteDownloading() - server "
                        << id << "went offline during download.";
                    m_serversActive.remove(id);
                }
                break;
        }
    }

    // No servers are doing anything. So we consider current state transition is complete
    if (m_serversActive.empty() && !m_serversIssued.empty() && m_clientUpdateTool->isDownloadComplete())
    {
        NX_VERBOSE(this) << "processRemoteDownloading() - download is complete";

        // Need to sync UI before showing modal dialogs.
        // Or we will get inconsistent UI state in the background.
        loadDataToUi();

        if (m_serversComplete.size() == m_serversIssued.size()
                && !m_serversComplete.empty()
                && !m_serversIssued.empty())
        {
            auto complete = m_serversComplete;
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
        else if (m_serversComplete.empty())
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
                auto serversToRetry = m_serversFailed;
                m_serverUpdateTool->requestStartUpdate(m_updateInfo.info);
                m_clientUpdateTool->downloadUpdate(m_updateInfo);
                setTargetState(WidgetUpdateState::downloading, serversToRetry);
            }
            else if (clicked == cancelUpdate)
            {
                auto serversToStop = m_serversIssued;
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
            auto skipFailed = messageBox->addButton(tr("Skip them"),
                QDialogButtonBox::RejectRole);
            auto cancelUpdate = messageBox->addButton(tr("Cancel Update"),
                QDialogButtonBox::RejectRole);
            messageBox->setEscapeButton(cancelUpdate);
            messageBox->exec();

            auto clicked = messageBox->clickedButton();
            if (clicked == tryAgain)
            {
                auto serversToRetry = m_serversFailed;
                m_serverUpdateTool->requestInstallAction(serversToRetry);
                setTargetState(WidgetUpdateState::readyInstall, serversToRetry);
            }
            else if (clicked == skipFailed)
            {
                // Start installing process for servers that have succeded downloading
                setTargetState(WidgetUpdateState::installing, m_serversComplete);
            }
            else if (clicked == cancelUpdate)
            {
                m_serverUpdateTool->requestStopAction();
                setTargetState(WidgetUpdateState::ready, {});
            }

            setTargetState(WidgetUpdateState::readyInstall, m_serversComplete);
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
    auto completeInstall = m_serverUpdateTool->getServersCompleteInstall();
    auto failed = m_serverUpdateTool->getServersInState(StatusCode::error);
    auto installing = m_serverUpdateTool->getServersInState(StatusCode::idle);

    for (auto id: completeInstall)
    {
        if (!m_serversComplete.contains(id))
        {
            NX_VERBOSE(this) << "processRemoteInstalling() - server "
                << id << "is has completed installation";
            m_serversComplete.insert(id);
        }
        m_serversActive.remove(id);
    }

    // No servers are installing anything right now. We should check if installation is complete.
    if (m_serversActive.empty())
    {
        bool clientInstallComplete = m_clientUpdateTool->isInstallComplete();
        if (!completeInstall.empty() && clientInstallComplete)
        {
            NX_VERBOSE(this) << "processRemoteInstalling() - installation is complete";
            setTargetState(WidgetUpdateState::complete);
            loadDataToUi();

            auto complete = m_serversComplete;
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Success);

            if (failed.empty())
            {
                messageBox->setText(tr("Update completed"));
                messageBox->setInformativeText(tr("Nx Witness Client will be restarted to the updated version."));
            }
            else
            {
                NX_VERBOSE(this) << "processRemoteInstalling() - servers" << failed << " have failed to install update";
                messageBox->setText(tr("Update completed, but some servers have failed an update"));
                injectResourceList(*messageBox, resourcePool()->getResourcesByIds(failed));
                messageBox->setInformativeText(tr("Nx Witness Client will be restarted to the updated version."));
            }

            auto installNow = messageBox->addButton(tr("OK"),
                QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
            messageBox->setEscapeButton(installNow);
            messageBox->exec();

            auto clicked = messageBox->clickedButton();
            if (clicked == installNow)
                completeInstallation(clientInstallComplete);
        }
        // No servers have installed updates
        else if (completeInstall.empty() && !failed.empty())
        {
            NX_VERBOSE(this) << "processRemoteInstalling() - installation has failed completely";
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Critical);
            messageBox->setText(tr("Failed to install updates to servers:"));
            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(failed));
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
}

void MultiServerUpdatesWidget::completeInstallation(bool clientUpdated)
{
    auto updatedProtocol = m_serverUpdateTool->getServersWithChangedProtocol();
    bool clientInstallerRequired = false;
    bool unholdConnection = !clientUpdated || clientInstallerRequired || !updatedProtocol.empty();

    if (clientUpdated && !clientInstallerRequired)
    {
        NX_VERBOSE(this) << "completeInstallation() - restarting the client";
        if (!m_clientUpdateTool->restartClient())
        {
            unholdConnection = true;
            QnConnectionDiagnosticsHelper::failedRestartClientMessage(this);
        }
        else
        {
            applauncher::api::scheduleProcessKill(QCoreApplication::applicationPid(), kProcessTerminateTimeoutMs);
            qApp->exit(0);
        }
    }

    if (unholdConnection)
        qnClientMessageProcessor->setHoldConnection(false);

    if (!updatedProtocol.empty())
    {
        NX_VERBOSE(this) << "completeInstallation() - servers" << updatedProtocol << "have new protocol. Forcing reconnect";
        menu()->trigger(action::DisconnectAction, {Qn::ForceRole, true});
    }
}

bool MultiServerUpdatesWidget::processRemoteChanges(bool force)
{
    // We gather here updated server status from updateTool
    // and change WidgetUpdateState state accordingly.

    // TODO: It could be moved to UpdateTool
    ServerUpdateTool::RemoteStatus remoteStatus;
    if (!m_serverUpdateTool->getServersStatusChanges(remoteStatus) && !force)
        return false;

    m_updatesModel->setUpdateStatus(remoteStatus);

    if (m_updateStateCurrent == WidgetUpdateState::initial)
        processRemoteInitialState();
    // We can move in processRemoteInitialState to another state,
    // and can process the new state right here.
    if (m_updateStateCurrent == WidgetUpdateState::downloading)
    {
        processRemoteDownloading();
    }
    else if (m_updateStateCurrent == WidgetUpdateState::installing)
    {
        processRemoteInstalling();
    }
    else if (m_updateStateCurrent == WidgetUpdateState::readyInstall)
    {
        auto idle = m_serverUpdateTool->getServersInState(StatusCode::idle);
        auto all = m_serverUpdateTool->getAllServers();
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

    if (m_updateStateCurrent == WidgetUpdateState::pushing)
    {
        if (state == ServerUpdateTool::OfflineUpdateState::done)
        {
            NX_VERBOSE(this) << "processUploaderChanges seems to be done";
            setTargetState(WidgetUpdateState::readyInstall, {});
        }
        else if (state == ServerUpdateTool::OfflineUpdateState::error)
        {
            NX_VERBOSE(this) << "processUploaderChanges failed to upload all packages";
            setTargetState(WidgetUpdateState::ready, {});
        }
    }
    return true;
}

void MultiServerUpdatesWidget::setTargetState(WidgetUpdateState state, QSet<QnUuid> targets)
{
    if (state != m_updateStateCurrent)
    {
        NX_VERBOSE(this) << "setTargetState() from"
            << toString(m_updateStateCurrent)
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
                if (m_rightPanelDownloadProgress.isNull())
                {
                    auto manager = context()->instance<WorkbenchProgressManager>();
                    m_rightPanelDownloadProgress = manager->add(tr("Downloading updates..."));
                }
                break;
            case WidgetUpdateState::pushing:
                if (m_rightPanelDownloadProgress.isNull())
                {
                    auto manager = context()->instance<WorkbenchProgressManager>();
                    m_rightPanelDownloadProgress = manager->add(tr("Pushing updates..."));
                }
                break;
            case WidgetUpdateState::readyInstall:
                stopProcess = true;
                break;
            case WidgetUpdateState::installing:
                m_serverUpdateTool->requestInstallAction(targets);
                if (m_clientUpdateTool->hasUpdate())
                    m_clientUpdateTool->installUpdate();
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
    m_updateStateCurrent = state;
    m_serversActive = targets;
    m_serversIssued = targets;
    m_serversComplete = {};
    m_serversFailed = {};
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

void MultiServerUpdatesWidget::syncUpdateCheck()
{
    bool isChecking = m_updateCheck.valid();
    bool hasEqualUpdateInfo = m_updatesModel->lowestInstalledVersion() >= m_updateInfo.getVersion();
    bool hasLatestVersion = ((m_updateInfo.isValid() || hasEqualUpdateInfo)
        || m_updateInfo.error == nx::update::InformationError::noNewVersion);

    if (m_updateStateCurrent != WidgetUpdateState::ready
        && m_updateStateCurrent != WidgetUpdateState::initial)
        hasLatestVersion = false;

    ui->manualDownloadButton->setVisible(!isChecking);
    ui->updateStackedWidget->setVisible(!isChecking);
    ui->selectUpdateTypeButton->setEnabled(!isChecking);

    // NOTE: I broke my brain here. Twice.
    if (isChecking)
    {
        ui->versionStackedWidget->setCurrentWidget(ui->checkingPage);
        ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
        ui->updateCheckMode->setVisible(false);
        hasLatestVersion = false;
    }
    else
    {
        ui->versionStackedWidget->setCurrentWidget(ui->versionPage);
        ui->releaseNotesLabel->setVisible(!m_updateInfo.info.releaseNotesUrl.isEmpty());

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

            ui->downloadButton->setVisible(true);

            if (!m_updateInfo.info.releaseNotesUrl.isEmpty())
            {
                ui->infoStackedWidget->setCurrentWidget(ui->releaseNotesPage);
            }
            else
            {
                ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
            }

            if (!m_updateInfo.info.description.isEmpty())
            {
                ui->releaseDescriptionLabel->setText(m_updateInfo.info.description);
                ui->releaseDescriptionLabel->show();
            }
            else
            {
                ui->releaseDescriptionLabel->hide();
            }
        }
        else if (hasLatestVersion)
        {
            ui->versionStackedWidget->setCurrentWidget(ui->latestVersionPage);
            ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
            ui->downloadButton->setVisible(false);
        }
        else if (!m_updateCheckError.isEmpty())
        {
            ui->errorLabel->setText(m_updateCheckError);
            ui->infoStackedWidget->setCurrentWidget(ui->errorPage);
            ui->downloadButton->setVisible(false);
        }

        bool browseUpdateVisible = false;
        if (/*!m_haveValidUpdate && */m_updateStateCurrent != WidgetUpdateState::readyInstall)
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

    bool showButton = m_updateSourceMode != UpdateSourceType::file &&
        (m_updateStateCurrent == WidgetUpdateState::ready || m_updateStateCurrent != WidgetUpdateState::initial);
    ui->manualDownloadButton->setVisible(showButton);
    auto version = versionText(m_updateInfo.getVersion());
    ui->targetVersionLabel->setText(version);
    m_updateLocalStateChanged = false;
}

void MultiServerUpdatesWidget::syncRemoteUpdateState()
{
    // This function gathers state status of update from remote servers and changes
    // UI state accordingly.

    bool hideInfo = m_updateStateCurrent == WidgetUpdateState::initial
        || m_updateStateCurrent == WidgetUpdateState::ready;
    hideStatusColumns(hideInfo);

    // Should we show progress for this UI state.
    bool hasProgress = false;
    // Title to be shown for this UI state.
    QString updateTitle;

    bool storageSettingsVisible = false;

    switch (m_updateStateCurrent)
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
            hasProgress = true;
            break;
        case WidgetUpdateState::readyInstall:
            updateTitle = tr("Ready to update to");
            ui->downloadButton->setText(tr("Install update"));
            break;
        case WidgetUpdateState::installing:
            updateTitle = tr("Updating to ...");
            hasProgress = true;
            break;
        case WidgetUpdateState::pushing:
            hasProgress = true;
            break;
        case WidgetUpdateState::complete:
            updateTitle = tr("System updated to");
            ui->downloadButton->setText(tr("Install update"));
            break;
        default:
            break;
    }

    ui->advancedUpdateSettings->setVisible(storageSettingsVisible);
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn,
        !storageSettingsVisible || !m_showStorageSettings);

    ui->cancelUpdateButton->setVisible(m_updateStateCurrent == WidgetUpdateState::readyInstall);

    // Updating title. That is the upper part of the window
    QWidget* selectedTitle = ui->selectUpdateTypePage;
    if (!updateTitle.isEmpty())
    {
        selectedTitle = ui->updatingPage;
        ui->updatingTitleLabel->setText(updateTitle);
    }

    if (selectedTitle && selectedTitle != ui->titleStackedWidget->currentWidget())
    {
        NX_VERBOSE(this) << "loadDataToUi - updating titleStackedWidget";
        ui->titleStackedWidget->setCurrentWidget(selectedTitle);
    }

    if (hasProgress)
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
    }
    else if (m_updateStateCurrent == WidgetUpdateState::complete)
    {
        ui->updateStackedWidget->setCurrentWidget(ui->updateCompletionPage);
    }
    else
    {
        ui->updateStackedWidget->setCurrentWidget(ui->updateControlsPage);
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
        syncUpdateCheck();
    }

    if (m_updateRemoteStateChanged)
        syncRemoteUpdateState();

    bool endOfTheWeek = QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek;
    ui->dayWarningBanner->setVisible(endOfTheWeek);

    // TODO: Update logic for auto check
    //setAutoUpdateCheckMode(qnGlobalSettings->isUpdateNotificationsEnabled());

    if (m_showDebugData)
    {
        QString combinePath = qnSettings->updateCombineUrl();
        if (combinePath.isEmpty())
            combinePath = QnAppInfo::updateGeneratorUrl();

        QStringList debugState = {
            QString("UpdateUrl=<a href=\"%1\">%1</a>").arg(qnSettings->updateFeedUrl()),
            QString("UpdateFeedUrl=<a href=\"%1\">%1</a>").arg(combinePath),
            QString("Widget=%1").arg(toString(m_updateStateCurrent)),
            QString("Widget source=%1, Update source=%2").arg(toString(m_updateSourceMode), toString(m_updateInfo.sourceType)),
            QString("UploadTool=%1").arg(toString(m_serverUpdateTool->getUploaderState())),
            QString("ClientTool=%1").arg(ClientUpdateTool::toString(m_clientUpdateTool->getState())),
            QString("validUpdate=%1").arg(m_haveValidUpdate),
            QString("targetVersion=%1").arg(m_updateInfo.info.version),
            QString("<a href=\"%1\">/ec2/updateStatus</a>").arg(m_serverUpdateTool->getUpdateStateUrl()),
            QString("<a href=\"%1\">/ec2/updateInformation</a>").arg(m_serverUpdateTool->getUpdateInformationUrl()),
            QString("<a href=\"%1\">/ec2/installedUpdateInformation</a>").arg(m_serverUpdateTool->getInstalledUpdateInfomationUrl()),
        };

        debugState << QString("lowestVersion=%1").arg(m_updatesModel->lowestInstalledVersion().toString());

        if (m_updateInfo.error != nx::update::InformationError::noError)
        {
            QString internalError = nx::update::toString(m_updateInfo.error);
            debugState << QString("updateInfoError=%1").arg(internalError);
        }
        ui->debugStateLabel->setText(debugState.join("<br>"));
    }

    ui->debugStateLabel->setVisible(m_showDebugData);
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StatusMessageColumn, !m_showDebugData);
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

    clearUpdateInfo();
    setTargetState(WidgetUpdateState::initial, {});
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

void MultiServerUpdatesWidget::hideStatusColumns(bool value)
{
    m_statusItemDelegate->setStatusVisible(!value);
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::ProgressColumn, value);
}

void MultiServerUpdatesWidget::checkForInternetUpdates()
{
    if (m_updateSourceMode != UpdateSourceType::internet)
        return;
    if (!isVisible())
        return;

    // No need to check for updates if we are already installing something.
    if (m_updateStateCurrent != WidgetUpdateState::initial && m_updateStateCurrent != WidgetUpdateState::ready)
        return;

    if (!m_updateCheck.valid())
    {
        clearUpdateInfo();
        QString updateUrl = qnSettings->updateFeedUrl();
        m_updateCheck = nx::update::checkLatestUpdate(updateUrl);
        syncUpdateCheck();
    }
}

void MultiServerUpdatesWidget::atModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& /*unused*/)
{
    // This forces custom editor to appear for every item in the table.
    for (int row = topLeft.row(); row <= bottomRight.row(); row++)
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
        case WidgetUpdateState::complete:
            return "Complete";
        default:
            return "Unknown";
    }
}

QString MultiServerUpdatesWidget::toString(ServerUpdateTool::OfflineUpdateState state)
{
    // These strings are internal and are not intended to be visible to regular user.
    switch(state)
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
    switch(mode)
    {
        case nx::update::UpdateSourceType::internet:
            return tr("Available Update");
        case nx::update::UpdateSourceType::internetSpecific:
            return tr("Specific Build...");
        case nx::update::UpdateSourceType::file:
            return tr("Browse for Update File...");
        case nx::update::UpdateSourceType::mediaservers:
            // This string should not appear at UI.
            return tr("Update from mediaservers");
    }
    return "Unknown update source mode";
}

} // namespace nx::vms::client::desktop
