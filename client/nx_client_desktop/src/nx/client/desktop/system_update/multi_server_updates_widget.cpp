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
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/widgets/views/resource_list_view.h>

#include <ui/models/resource/resource_list_model.h>

#include <update/low_free_space_warning.h>

#include <utils/applauncher_utils.h>
#include <utils/common/html.h>
#include <utils/common/scoped_value_rollback.h>
#include <nx/client/desktop/utils/upload_manager.h>
#include <utils/connection_diagnostics_helper.h>

#include "server_update_tool.h"
#include "server_updates_model.h"
#include "server_status_delegate.h"

using namespace nx::client::desktop::ui;

/**
 * Description for UI flow can be found there:
 * https://docs.google.com/drawings/d/1jJKBDCAyI765XXbUjcnx165AFzmUT4moowgq9letkNE
 */

namespace {

const int kLongInstallWarningTimeoutMs = 2 * 60 * 1000; // 2 minutes
// Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
const quint32 kProcessTerminateTimeoutMs = 15000;

const int kTooLateDayOfWeek = Qt::Thursday;

const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour

const int kMaxLabelWidth = 400; // pixels

const int kVersionLabelFontSizePixels = 24;
const int kVersionLabelFontWeight = QFont::DemiBold;

// Height limit for servers list in dialog box with update report
static constexpr int kSectionHeight = 150;
static constexpr int kSectionMinHeight = 30;

constexpr auto kLatestVersionBannerLabelFontSizePixels = 22;
constexpr auto kLatestVersionBannerLabelFontWeight = QFont::Light;

const int kLinkCopiedMessageTimeoutMs = 2000;

/* N-dash 5 times: */
const QString kNoVersionNumberText = QString::fromWCharArray(L"\x2013\x2013\x2013\x2013\x2013");

QString elidedText(const QString& text, const QFontMetrics& fontMetrics)
{
    QString result;

    for (const auto& line : text.split(L'\n', QString::KeepEmptyParts))
    {
        if (result.isEmpty())
            result += L'\n';

        result += fontMetrics.elidedText(line, Qt::ElideMiddle, kMaxLabelWidth);
    }

    return result;
}

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

namespace nx {
namespace client {
namespace desktop {

MultiServerUpdatesWidget::MultiServerUpdatesWidget(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    ui(new Ui::MultiServerUpdatesWidget)
{
    ui->setupUi(this);

    m_updatesTool.reset(new ServerUpdateTool(this));
    m_updatesModel.reset(new ServerUpdatesModel(this));
    m_updateManager.reset(new UpdatesManager(commonModule()));
    m_uploadManager.reset(new UploadManager());

    // I wonder whether it is blocking call?
    m_updateManager->atServerStart();

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

    /* TODO: We should sort by:
     *  - online status
     *  - connectivity
     *  - free space on the server
     */
    m_sortedModel.reset(new QnSortedServerUpdatesModel(this));
    m_sortedModel->setSourceModel(m_updatesModel.get());
    ui->tableView->setModel(m_sortedModel.get());
    auto delegate = new ServerStatusItemDelegate(ui->tableView);
    ui->tableView->setItemDelegateForColumn(ServerUpdatesModel::ProgressColumn,
        delegate);
    connect(delegate, &ServerStatusItemDelegate::updateItemCommand,
        this, &MultiServerUpdatesWidget::at_updateItemCommand);

    connect(m_sortedModel.get(), &QnSortedServerUpdatesModel::dataChanged, this,
        &MultiServerUpdatesWidget::at_modelDataChanged);

    m_updatesModel->setResourceFeed(resourcePool());
    m_updatesTool->setResourceFeed(resourcePool());

    // the column does not matter because the model uses column-independent sorting
    m_sortedModel->sort(0);

    ui->tableView->setEditTriggers(QAbstractItemView::AllEditTriggers);
    if (auto horizontalHeader = ui->tableView->horizontalHeader())
    {
        horizontalHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
        horizontalHeader->setSectionResizeMode(ServerUpdatesModel::ProgressColumn,
            QHeaderView::Stretch);
        horizontalHeader->setSectionsClickable(false);
    }

    ui->refreshButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    connect(ui->refreshButton, &QPushButton::clicked,
        this, &MultiServerUpdatesWidget::at_clickCheckForUpdates);

    // TODO: Get rid of this button?
    ui->getAndInstall->setEnabled(false);
    connect(ui->getAndInstall, &QPushButton::clicked,
        this, &MultiServerUpdatesWidget::at_clickUpdateServers);

    connect(ui->cancelProgressAction, &QPushButton::clicked, this,
        &MultiServerUpdatesWidget::at_cancelCurrentAction);

    connect(ui->cancelUpdateButton, &QPushButton::clicked, this,
        &MultiServerUpdatesWidget::at_cancelCurrentAction);

    connect(ui->downloadButton, &QPushButton::clicked,
        this, &MultiServerUpdatesWidget::at_startUpdateAction);
    connect(ui->autoCheckUpdates, &QCheckBox::stateChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->releaseNotesLabel, &QLabel::linkActivated, this, [this]()
        {
            if (!m_releaseNotesUrl.isEmpty())
                QDesktopServices::openUrl(m_releaseNotesUrl.toQUrl());
        });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
        this, &MultiServerUpdatesWidget::at_clickCheckForUpdates);

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

    ui->releaseNotesLabel->setText(lit("<a href='notes'>%1</a>").arg(tr("Release notes")));
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

    // We have disabled update tool right now
    //at_tool_stageChanged(QnFullUpdateStage::Init);

    ui->manualDownloadButton->hide();
    ui->manualDownloadButton->setIcon(qnSkin->icon(lit("text_buttons/download.png")));
    ui->manualDownloadButton->setForegroundRole(QPalette::WindowText);

    initDownloadActions();
    initDropdownActions();

    setModeLatestAvailable();

    // Starting timer that tracks remote update state
    m_stateCheckTimer.reset(new QTimer(this));
    m_stateCheckTimer->setSingleShot(false);
    m_stateCheckTimer->start(1000);
    connect(m_stateCheckTimer.get(), &QTimer::timeout,
        this, &MultiServerUpdatesWidget::at_stateCheckTimer);

    // Force update when we open dialog. Is there some sort of stored config?
    checkForUpdates(true);

    loadDataToUi();
}

MultiServerUpdatesWidget::~MultiServerUpdatesWidget()
{
}

// Moved from ServerUpdateTool
bool MultiServerUpdatesWidget::cancelUpdatesCheck()
{
    // TODO: reimplement it using new update manager
    return true;
}

void MultiServerUpdatesWidget::at_stateCheckTimer()
{
    NX_ASSERT(m_updatesTool);
    if (isHidden())
        return;
    auto status = m_updateManager->status();
    if (status.state != m_lastStatus)
    {
        qDebug() << "Local update state is changing from" << toString(m_lastStatus) << "to" << toString(status.state);
        if (m_lastStatus == LocalStatusCode::checking || m_checking)
        {
            bool doneChecking = false;
            if (status.state == LocalStatusCode::available)
            {
                if (!status.targets.empty())
                {
                    nx::api::TargetVersionWithEula tv = status.targets.front();
                    m_localUpdateInfo.latestVersion = tv.targetVersion;
                    m_haveUpdate = true;
                    doneChecking = true;
                }
            }
            else if (status.state == LocalStatusCode::checking)
            {
                // We are still checking
            }
            else
            {
                m_haveUpdate = false;
                doneChecking = true;
                qDebug() << "check for update error message" << status.message;
                // Some default
            }

            if (doneChecking)
            {

                m_updateLocalStateChanged = true;
                m_checking = false;

                //QnSoftwareVersion version = status.version.isNull()
                //    ? m_localUpdateInfo.latestVersion
                //    : result.version;

                auto displayVersion = versionText(m_localUpdateInfo.latestVersion);
                ui->targetVersionLabel->setText(displayVersion);
                ui->infoStackedWidget->setCurrentWidget(ui->releaseNotesPage);
            }
        }

        m_lastStatus = status.state;
    }


    // Maybe we should not call it right here
    m_updatesTool->requestRemoteUpdateState();

    if (m_updatesTool->hasRemoteChanges() || m_updateLocalStateChanged)
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
    qDebug() << "MultiServerUpdatesWidget::forcedUpdate()";
    checkForUpdates(m_localFileName.isEmpty());
}

void MultiServerUpdatesWidget::setUpdateSourceMode(UpdateSourceMode mode)
{
    m_updateSourceMode = mode;
    m_updateLocalStateChanged = true;

    loadDataToUi();
}

void MultiServerUpdatesWidget::setModeLocalFile()
{
    m_localFileName = QFileDialog::getOpenFileName(this,
        tr("Select Update File..."),
        QString(), tr("Update Files (*.zip)"),
        0,
        QnCustomFileDialog::fileDialogOptions());

    if (m_localFileName.isEmpty())
        return;

    m_updatesModel->setUpdateTarget(nx::utils::SoftwareVersion(), {});
    setUpdateSourceMode(UpdateSourceMode::LocalFile);
}

void MultiServerUpdatesWidget::setModeSpecificBuild()
{
    QnBuildNumberDialog dialog(this);
    if (!dialog.exec())
        return;

    nx::utils::SoftwareVersion version = qnStaticCommon->engineVersion();
    m_targetVersion = nx::utils::SoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber());
    m_targetChangeset = dialog.changeset();
    m_localFileName = QString();

    setUpdateSourceMode(UpdateSourceMode::SpecificBuild);
}

void MultiServerUpdatesWidget::setModeLatestAvailable()
{
    m_targetVersion = nx::utils::SoftwareVersion();
    m_targetChangeset = QString();
    m_localFileName = QString();

    api::Updates2StatusData status = m_updateManager->status();

    ui->targetVersionLabel->setText(versionText(m_localUpdateInfo.latestVersion));
    ui->selectUpdateTypeButton->setText(tr("Latest Available Update"));

    setUpdateSourceMode(UpdateSourceMode::LatestVersion);
}

void MultiServerUpdatesWidget::initDropdownActions()
{
    auto selectUpdateTypeMenu = new QMenu(this);
    selectUpdateTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    /*
    auto defaultAction = selectUpdateTypeMenu->addAction(tr("Latest Available Update"),
        [this]()
        {
            setModeLatestAvailable();
        });*/

    selectUpdateTypeMenu->addAction(tr("Specific Build..."),
        [this]()
        {
            setModeSpecificBuild();
        });

    selectUpdateTypeMenu->addAction(tr("Browse for Update File..."),
        [this]()
        {
            setModeLocalFile();
        });

    ui->selectUpdateTypeButton->setMenu(selectUpdateTypeMenu);
}

void MultiServerUpdatesWidget::initDownloadActions()
{
    auto downloadLinkMenu = new QMenu(this);
    downloadLinkMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    downloadLinkMenu->addAction(tr("Download in External Browser"),
        [this]()
        {
            QSet<QnUuid> targets = m_updatesModel->getAllServers();
            auto url = m_updatesTool->generateUpdatePackageUrl(m_targetVersion, m_targetChangeset, targets, resourcePool()).toString();
            QDesktopServices::openUrl(url);
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            QSet<QnUuid> targets = m_updatesModel->getAllServers();
            auto url = m_updatesTool->generateUpdatePackageUrl(m_targetVersion, m_targetChangeset, targets, resourcePool()).toString();
            m_updateSourcePath = url;
            qApp->clipboard()->setText(m_updateSourcePath.toString());

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

void MultiServerUpdatesWidget::at_updateItemCommand(std::shared_ptr<UpdateItem> item)
{
    if (!item)
        return;

    if (!item->server)
        return;

    auto id = item->server->getId();
    auto state = item->state;

    if (m_updateStateCurrent == WidgetUpdateState::RemoteDownloading)
    {
        // Server was in 'downloading' state and we decided to cancel it
        if (state == StatusCode::downloading && m_serversActive.contains(id))
        {
            m_serversActive.remove(id);
            m_serversCanceled.insert(id);
            item->skipped = true;
            m_updatesTool->requestUpdateAction(
                nx::api::Updates2ActionData::ActionCode::stop, { id });
        }

        // Server was in 'available' state and we decided to retry download.
        if (state == StatusCode::available && m_serversCanceled.contains(id))
        {
            m_serversCanceled.remove(id);
            m_serversActive.insert(id);
            item->skipped = false;
            // TODO: Should we really stop it?
            m_updatesTool->requestUpdateAction(
                nx::api::Updates2ActionData::ActionCode::download, { id }, m_localUpdateInfo.latestVersion);
        }

        // Server was in 'error' state and we decided to retry.
        if (state == StatusCode::error && m_serversFailed.contains(id))
        {
            m_serversFailed.remove(id);
            m_serversActive.insert(id);
            // TODO: Should we really stop it?
            m_updatesTool->requestUpdateAction(
                nx::api::Updates2ActionData::ActionCode::download, { id }, m_localUpdateInfo.latestVersion);
        }

        if (m_serversActive.empty())
        {
            // Moving to 'Initial' state temporary.
            // processRemoteChanges will pick the next state.
            moveTowardsState(WidgetUpdateState::Initial, {});
            processRemoteChanges(true);
        }
    }
}

void MultiServerUpdatesWidget::at_clickUpdateServers()
{
    /*
    auto targets = m_updatesModel->getServersInState(StatusCode::readyToInstall);
    if (targets.empty())
        return;

    m_serversUpdating = targets;
    auto action = nx::api::Updates2ActionData::ActionCode::install;
    m_updateTool->requestUpdateAction(action, targets);
    */

    /*
    if (m_localFileName.isEmpty())
        m_updateTool->startUpdate(m_targetVersion);
    else
        m_updateTool->startUpdate(m_localFileName);
    */
}

void MultiServerUpdatesWidget::at_clickCheckForUpdates()
{
    checkForUpdates(true);
}

void MultiServerUpdatesWidget::at_startUpdateAction()
{
    // Clicked 'Download' button. Should start download process here

    NX_ASSERT(m_updatesTool);

    if (m_updateStateCurrent == WidgetUpdateState::Ready)
    {
        // Start download process
        // Move to state 'Downloading'
        // At this state should check remote state untill everything is complete

        auto targets = m_updatesModel->getServersInState(StatusCode::available);
        if (targets.empty())
        {
            qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - no servers can download update";
            return;
        }

        qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - sending 'download' command";
        auto offlineServers = m_updatesModel->getOfflineServers();
        if (!offlineServers.empty())
        {
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Warning);
            messageBox->setText(tr("Some servers are offline and will not be updated. Skip them?"));
            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(offlineServers));
            auto skip = messageBox->addCustomButton(QnMessageBoxCustomButton::Skip,
                QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);
            auto cancel = messageBox->addButton(QDialogButtonBox::Cancel);

            messageBox->exec();
            auto clicked = messageBox->clickedButton();
            if (clicked == cancel)
                return;
        }

        auto incompatible = m_updatesModel->getLegacyServers();
        if (!incompatible.empty())
        {
            nx::utils::SoftwareVersion version = m_targetVersion;
            /*
            m_updatesTool->startOnlineClientUpdate(incompatible, m_targetVersion, false);
            // Run compatibility update
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            messageBox->setIcon(QnMessageBoxIcon::Warning);
            messageBox->setText(tr("Some servers have incompatible versions and will not be updated"));
            injectResourceList(*messageBox, resourcePool()->getResourcesByIds(incompatible));
            auto ok = messageBox->addButton(QDialogButtonBox::Ok);
            messageBox->setEscapeButton(ok);
            messageBox->exec();*/
        }

        moveTowardsState(WidgetUpdateState::RemoteDownloading, targets);
        auto action = nx::api::Updates2ActionData::ActionCode::download;
        m_updatesTool->requestUpdateAction(action, targets, m_localUpdateInfo.latestVersion);
    }
    else if (m_updateStateCurrent == WidgetUpdateState::ReadyInstall)
    {
        auto targets = m_updatesModel->getServersInState(StatusCode::readyToInstall);
        if (targets.empty())
        {
            qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - no server can install anything";
            return;
        }

        qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - starting installation";
        moveTowardsState(WidgetUpdateState::Installing, targets);
        auto action = nx::api::Updates2ActionData::ActionCode::install;
        m_updatesTool->requestUpdateAction(action, targets, m_localUpdateInfo.latestVersion);
    }
    else
    {
        qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - invalid widget state for download command";
    }
}

bool MultiServerUpdatesWidget::at_cancelCurrentAction()
{
    // TODO: Should be able to cancel update for specific server

    // Cancel all the downloading
    if (m_updateStateCurrent == WidgetUpdateState::RemoteDownloading)
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent);
        auto serversToCancel = m_serversIssued;
        m_updatesTool->requestUpdateAction(
            nx::api::Updates2ActionData::ActionCode::stop, serversToCancel);
        moveTowardsState(WidgetUpdateState::Ready, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::Installing)
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent);
        // Should send 'cancel' command to all the?
        auto serversToCancel = m_updatesModel->getServersInState(StatusCode::installing);
        m_updatesTool->requestUpdateAction(
            nx::api::Updates2ActionData::ActionCode::stop, serversToCancel);
        moveTowardsState(WidgetUpdateState::Initial, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::ReadyInstall)
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent);
        auto serversToCancel = m_updatesModel->getServersInState(StatusCode::readyToInstall);
        m_updatesTool->requestUpdateAction(
            nx::api::Updates2ActionData::ActionCode::stop, serversToCancel);
        moveTowardsState(WidgetUpdateState::Initial, {});
    }
    else
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent) << ": not implemented";
    }
    // Spec says that we can not cancel anything when we began installing stuff
    return false;
}

bool MultiServerUpdatesWidget::canCancelUpdate() const
{
    /* BROKEN
    if (m_updateTool->isUpdating())
        return m_updateTool->canCancelUpdate();
        */
    // TODO: We can not cancel update for the servers, that are in 'installing' state.

    return true;
}

bool MultiServerUpdatesWidget::isUpdating() const
{
    // TODO: Get proper value
    // m_updatesTool->isUpdating();
    return false;
}

MultiServerUpdatesWidget::ProgressInfo MultiServerUpdatesWidget::calculateActionProgress() const
{
    ProgressInfo result;
    for (auto id : m_serversActive)
    {
        auto item = m_updatesModel->findItemById(id);
        if (!item)
            continue;

        result.current += item->progress;
        result.max += 100;
    }
    return result;
}

void MultiServerUpdatesWidget::processRemoteInitialState()
{
    /* Some servers can ba in state:
    - ready to install
    - downloading
    - installing
    */
    auto downloaded = m_updatesModel->getServersInState(StatusCode::readyToInstall);
    auto downloading = m_updatesModel->getServersInState(StatusCode::downloading);
    auto installing = m_updatesModel->getServersInState(StatusCode::installing);

    qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - we are in initial state and finally got status for remote system";

    if (!downloading.empty())
    {
        qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - some servers are downloading an update";
        moveTowardsState(WidgetUpdateState::RemoteDownloading, {});
    }
    else if (!installing.empty())
    {
        qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - some servers are installing an update";
        moveTowardsState(WidgetUpdateState::Installing, {});
    }
    else if (!downloaded.empty())
    {
        qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - some servers have already downloaded an update";
        moveTowardsState(WidgetUpdateState::ReadyInstall, {});
    }
    else
    {
        moveTowardsState(WidgetUpdateState::Ready, {});
    }
}

void MultiServerUpdatesWidget::processRemoteDownloading(
    const ServerUpdateTool::UpdateStatus& remoteStatus)
{
    // State changes for every server are quite different for each UI state,
    // so we do this for every branch
    for (const auto& status : remoteStatus)
    {
        auto id = status.first;

        switch (status.second.state)
        {
        case StatusCode::readyToInstall:
            qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - server " << id << "completed downloading";
            m_serversComplete.insert(id);
            m_serversActive.remove(id);
            break;
        case StatusCode::error:
            if (m_serversIssued.contains(id) && m_serversActive.contains(id))
            {
                qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - server " << id << " failed to download: " << status.second.message;
                m_serversFailed.insert(id);
                m_serversActive.remove(id);
            }
        default:
            break;
        }
    }

    // No servers are doing anything. So we consider current state transition is complete
    if (m_serversActive.empty())
    {
        qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - download is complete";

        if (m_serversComplete.size() == m_serversIssued.size())
        {
            QScopedPointer<QnSessionAwareMessageBox> messageBox(new QnSessionAwareMessageBox(this));
            // 1. Everything is complete
            messageBox->setIcon(QnMessageBoxIcon::Success);
            messageBox->setText(tr("Updates downloaded"));
            // S|Install now| |Later|
            moveTowardsState(WidgetUpdateState::ReadyInstall, m_serversComplete);
            auto installNow = messageBox->addButton(tr("Install now"),
                QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
            auto installLater = messageBox->addButton(tr("Later"), QDialogButtonBox::RejectRole);
            messageBox->setEscapeButton(installLater);
            messageBox->exec();

            auto clicked = messageBox->clickedButton();
            if (clicked == installNow)
            {
                auto serversToUpdate = m_serversComplete;
                m_updatesTool->requestUpdateAction(
                    nx::api::Updates2ActionData::ActionCode::install, serversToUpdate,
                    m_localUpdateInfo.latestVersion);
                moveTowardsState(WidgetUpdateState::Installing, serversToUpdate);
            }
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
                m_updatesTool->requestUpdateAction(
                    nx::api::Updates2ActionData::ActionCode::download,
                    serversToRetry,
                    m_localUpdateInfo.latestVersion);
                moveTowardsState(WidgetUpdateState::RemoteDownloading, serversToRetry);
            }
            else if (clicked == cancelUpdate)
            {
                auto serversToStop = m_serversIssued;
                m_updatesTool->requestUpdateAction(
                    nx::api::Updates2ActionData::ActionCode::stop, m_serversIssued);
                moveTowardsState(WidgetUpdateState::Ready, {});
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
                m_updatesTool->requestUpdateAction(
                    nx::api::Updates2ActionData::ActionCode::install, serversToRetry,
                    m_localUpdateInfo.latestVersion);
                moveTowardsState(WidgetUpdateState::ReadyInstall, serversToRetry);
            }
            else if (clicked == skipFailed)
            {
                // Start installing process for servers that have succeded downloading
                auto serversToIntall = m_serversIssued;
                m_updatesTool->requestUpdateAction(
                    nx::api::Updates2ActionData::ActionCode::install, serversToIntall,
                    m_localUpdateInfo.latestVersion);
                moveTowardsState(WidgetUpdateState::Installing, serversToIntall);
            }
            else if (clicked == cancelUpdate)
            {
                m_updatesTool->requestUpdateAction(
                    nx::api::Updates2ActionData::ActionCode::stop, m_serversIssued);
                moveTowardsState(WidgetUpdateState::Ready, {});
            }

            moveTowardsState(WidgetUpdateState::ReadyInstall, m_serversComplete);
        }
        // TODO: Check servers that are online, but skipped + m_serversCanceled
        // TODO: Show dialog "Some servers were skipped."
        {
            //messageBox->setText(tr("Only servers which have update packages downloaded will be updated."));
            //messageBox->setInformativeText(tr("Not updated servers may be unavailable if you are connected to the updated server, and vice versa."));
        }
    }
}

void MultiServerUpdatesWidget::processRemoteInstalling(
    const ServerUpdateTool::UpdateStatus& remoteStatus)
{
    // TODO: Implement it
    for (const auto& status : remoteStatus)
    {
        auto id = status.first;
        auto state = status.second.state;

        switch (status.second.state)
        {
        case StatusCode::preparing:
        case StatusCode::installing:
            // Nothing to do here. We just wait until it completes
            break;
        case StatusCode::readyToInstall:
            qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - server " << id << "completed downloading";
            m_serversComplete.insert(id);
            m_serversActive.remove(id);
            break;
        case StatusCode::error:
            if (m_serversIssued.contains(id) && m_serversActive.contains(id))
            {
                qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - server " << id << " failed to download: " << status.second.message;
                m_serversFailed.insert(id);
                m_serversActive.remove(id);
            }
        default:
            break;
        }
    }
}

bool MultiServerUpdatesWidget::processRemoteChanges(bool force)
{
    // We gather here updated server status from updateTool
    // and change WidgetUpdateState state accordingly

    ServerUpdateTool::UpdateStatus remoteStatus;
    if (!m_updatesTool->getServersStatusChanges(remoteStatus) && !force)
        return false;

    //qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - got update for " << remoteStatus.size() << " servers";
    m_updatesModel->setUpdateStatus(remoteStatus);

    if (m_updateStateCurrent == WidgetUpdateState::Initial)
    {
        processRemoteInitialState();
    }

    if (m_updateStateCurrent == WidgetUpdateState::RemoteDownloading)
    {
        processRemoteDownloading(remoteStatus);
    }
    else if (m_updateStateCurrent == WidgetUpdateState::Installing)
    {
        processRemoteInstalling(remoteStatus);
    }

    m_updateRemoteStateChanged = true;

    return true;
}

bool MultiServerUpdatesWidget::processLegacyChanges(bool force)
{
    // We gather here updated server status from updateTool
    // and change WidgetUpdateState state accordingly

    /*
    ServerUpdateTool::LegacyUpdateStatus status;
    if (!m_updatesTool->getLegacyUpdateStatusChanges(status) && !force)
        return false;

    if (m_updateStateCurrent == WidgetUpdateState::LegacyUpdating)
    {
        for (const auto& pair : status.peerProgress)
        {
            if (auto item = m_updatesModel->findItemById(pair.first))
                item->progress = pair.second;
        }

        for (const auto& pair : status.peerStage)
        {
            if (auto item = m_updatesModel->findItemById(pair.first))
            {
                switch (pair.second)
                {
                case QnPeerUpdateStage::Init:
                    item->statusMessage = lit("Init");
                    break;
                case QnPeerUpdateStage::Download:
                    item->statusMessage = lit("Download");
                    break;
                case QnPeerUpdateStage::Push:
                    item->statusMessage = lit("Push");
                    break;
                case QnPeerUpdateStage::Install:
                    item->statusMessage = lit("Install");
                    break;
                }
            }
        }
    }
    */
    return true;
}

void MultiServerUpdatesWidget::moveTowardsState(WidgetUpdateState state, QSet<QnUuid> targets)
{
    // TODO: We could try to move to 'far' states, with no direct transition.
    // Should invent something for such case.
    m_updateStateCurrent = state;
    m_serversActive = targets;
    m_serversIssued = targets;
    m_serversComplete = {};
    m_serversFailed = {};
    m_updateRemoteStateChanged = true;
}

void MultiServerUpdatesWidget::syncUpdateSource()
{
    // Code from the former 'updateAccent'
    // 'Update' button accented by default if update is possible
    //bool accented = m_lastUpdateCheckResult.result == QnCheckForUpdateResult::UpdateFound;

    if (m_checking)
    {
        ui->manualDownloadButton->hide();
        ui->refreshButton->hide();
        ui->versionStackedWidget->setCurrentWidget(ui->checkingPage);
        ui->updateStackedWidget->hide();
    }
    else
    {
        ui->manualDownloadButton->show();
        ui->refreshButton->show();
        ui->updateStackedWidget->show();

        if (m_haveUpdate)
            ui->versionStackedWidget->setCurrentWidget(ui->versionPage);
        else
            ui->versionStackedWidget->setCurrentWidget(ui->latestVersionPage);
    }

    ui->releaseNotesLabel->setVisible(!m_releaseNotesUrl.isEmpty());
    ui->manualDownloadButton->setEnabled(m_haveUpdate);

    if (m_haveUpdate)
        setAccentStyle(ui->downloadButton);
    else
        resetButtonStyle(ui->downloadButton);

    // Code from updateButtonText()
    QString text = m_updateSourceMode == UpdateSourceMode::SpecificBuild
        ? tr("Update to Specific Build")
        : tr("Update System");
    ui->getAndInstall->setText(text);

    // Code from updateVersionPage()
    bool hasLatestVersion = m_updateSourceMode == UpdateSourceMode::LatestVersion
        && (m_localUpdateInfo.latestVersion.isNull() || m_updatesModel->lowestInstalledVersion() >= m_localUpdateInfo.latestVersion);

    /*
    ui->versionStackedWidget->setCurrentWidget(hasLatestVersion
        ? ui->latestVersionPage
        : ui->versionPage);*/

    if (hasLatestVersion)
        ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);

    if (m_latestVersionChanged)
    {
        bool showButton = m_updateSourceMode != UpdateSourceMode::LocalFile
            && !hasLatestVersion;

        ui->manualDownloadButton->setVisible(showButton);

        m_latestVersionChanged = false;
    }

    switch (m_updateSourceMode)
    {
    case UpdateSourceMode::LatestVersion:
        ui->targetVersionLabel->setText(versionText(m_localUpdateInfo.latestVersion));
        ui->selectUpdateTypeButton->setText(tr("Latest Available Update"));
        break;

    case UpdateSourceMode::SpecificBuild:
        ui->targetVersionLabel->setText(m_targetVersion.toString());
        ui->selectUpdateTypeButton->setText(tr("Selected Version"));
        break;

    case UpdateSourceMode::LocalFile:
        ui->targetVersionLabel->setText(kNoVersionNumberText);
        ui->selectUpdateTypeButton->setText(tr("Selected Update File"));
        break;
    }

    m_updateLocalStateChanged = false;

    ui->selectUpdateTypeButton->setEnabled(!m_checking);
}

void MultiServerUpdatesWidget::syncRemoteUpdateState()
{
    // This function gathers state status of update from remote servers and changes
    // UI state accordingly.
    if (m_updateStateCurrent == WidgetUpdateState::ReadyInstall)
        ui->downloadButton->setText(tr("Install update"));
    else
        ui->downloadButton->setText(tr("Download"));

    bool hideInfo = m_updateStateCurrent == WidgetUpdateState::Initial
        || m_updateStateCurrent == WidgetUpdateState::Ready;
    hideStatusColumns(hideInfo);

    //
    QWidget* selectedUpdateStatus = ui->updateControlsPage;
    // Should we calculate progress for this UI state
    bool hasProgress = false;
    // Title to be shown for this UI state
    QString updateTitle;

    switch (m_updateStateCurrent)
    {
    case WidgetUpdateState::Initial:
        break;
    case WidgetUpdateState::Ready:
        break;
    case WidgetUpdateState::LocalDownloading:
        updateTitle = tr("Updating to ...");
        hasProgress = true;
        selectedUpdateStatus = ui->updateProgressPage;
        break;
    case WidgetUpdateState::RemoteDownloading:
        updateTitle = tr("Updating to ...");
        hasProgress = true;
        selectedUpdateStatus = ui->updateProgressPage;
        break;
    case WidgetUpdateState::ReadyInstall:
        updateTitle = tr("Ready to update to");
        break;
    case WidgetUpdateState::Installing:
        updateTitle = tr("Updating to ...");
        hasProgress = true;
        selectedUpdateStatus = ui->updateProgressPage;
        break;
    }

    ui->cancelUpdateButton->setVisible(m_updateStateCurrent == WidgetUpdateState::ReadyInstall);
    ui->getAndInstall->setVisible(m_updateStateCurrent != WidgetUpdateState::ReadyInstall);

    // Updating title. That is the upper part of the window
    QWidget* selectedTitle = ui->selectUpdateTypePage;
    if (!updateTitle.isEmpty())
    {
        selectedTitle = ui->updatingPage;
        ui->updatingTitleLabel->setText(updateTitle);
    }

    if (selectedTitle && selectedTitle != ui->titleStackedWidget->currentWidget())
    {
        qDebug() << "MultiServerUpdatesWidget::loadDataToUi - updating titleStackedWidget";
        ui->titleStackedWidget->setCurrentWidget(selectedTitle);
    }

    // Next part of the window. Can switch between progress page and commands page
    if (selectedUpdateStatus && selectedUpdateStatus != ui->updateStackedWidget->currentWidget())
    {
        qDebug() << "MultiServerUpdatesWidget::loadDataToUi - updating updateStackedWidget";
        ui->updateStackedWidget->setCurrentWidget(selectedUpdateStatus);
    }

    if (hasProgress)
    {
        ProgressInfo info = calculateActionProgress();
        ui->actionProgess->setValue(info.current);
        ui->actionProgess->setMaximum(info.max);
        //ui->actionProgess->setFormat(status);
    }
    //qDebug() << "MultiServerUpdatesWidget::loadDataToUi - done with m_updateStateChanged";
    m_updateRemoteStateChanged = false;
}

void MultiServerUpdatesWidget::loadDataToUi()
{
    // Synchronises internal state and UI widget state.
    NX_ASSERT(m_updatesTool);

    processRemoteChanges();
    processLegacyChanges();

    // Update UI state to match modes: {SpecificBuild;LatestVersion;LocalFile}
    if (m_updateLocalStateChanged)
    {
        qDebug() << "MultiServerUpdatesWidget::loadDataToUi - m_updateSourceModeChanged";
        syncUpdateSource();
    }

    // This block is about update state from another peers
    if (m_updateRemoteStateChanged)
    {
        syncRemoteUpdateState();
    }

    ui->dayWarningBanner->setVisible(QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek);
    ui->autoCheckUpdates->setChecked(qnGlobalSettings->isUpdateNotificationsEnabled());


    auto state = m_updateManager->status();
    ui->debugStateLabel->setText(lit("Widget=")+toString(m_updateStateCurrent)+" local=" + toString(state.state));
}

void MultiServerUpdatesWidget::applyChanges()
{
    qnGlobalSettings->setUpdateNotificationsEnabled(ui->autoCheckUpdates->isChecked());
    qnGlobalSettings->synchronizeNow();
}

void MultiServerUpdatesWidget::discardChanges()
{
    //
    //if (!m_updatesTool->isUpdating())
    //    return;

    if (canCancelUpdate())
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
            at_cancelCurrentAction();
    }
    else
    {
        QnMessageBox::warning(this,
            tr("Update cannot be canceled at this stage"),
            tr("Please wait until it is finished."));
    }
}

bool MultiServerUpdatesWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return qnGlobalSettings->isUpdateNotificationsEnabled() != ui->autoCheckUpdates->isChecked();
}

bool MultiServerUpdatesWidget::canApplyChanges() const
{
    // TODO: #GDM now this prevents other tabs from saving their changes
    return !isUpdating();
}

bool MultiServerUpdatesWidget::canDiscardChanges() const
{
    // TODO: #GDM now this prevents other tabs from discarding their changes
    return canCancelUpdate();
}

void MultiServerUpdatesWidget::autoCheckForUpdates()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAutoUpdateCheck < kAutoCheckIntervalMs)
        return;

    /* Do not recheck specific build. */
    if (!m_targetVersion.isNull())
        return;

    /* Do not check if local file is selected. */
    if (!m_localFileName.isEmpty())
        return;

    checkForUpdates(true);
}

bool MultiServerUpdatesWidget::restartClient(const nx::utils::SoftwareVersion& version)
{
    /* Try to run applauncher if it is not running. */
    if (!applauncher::checkOnline())
        return false;

    const auto result = applauncher::restartClient(version);
    if (result == applauncher::api::ResultType::ok)
        return true;

    static const int kMaxTries = 5;
    for (int i = 0; i < kMaxTries; ++i)
    {
        QThread::msleep(100);
        qApp->processEvents();
        if (applauncher::restartClient(version) == applauncher::api::ResultType::ok)
            return true;
    }
    return false;
}

void MultiServerUpdatesWidget::hideStatusColumns(bool value)
{
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::ProgressColumn, value);
    ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StatusColumn, value);
}

void MultiServerUpdatesWidget::checkForUpdates(bool fromInternet)
{
    if (m_checking)
        return;

    // We should manually check its status to know whether there are actually any updates
    auto status = m_updateManager->status();
    switch (status.state)
    {
        case LocalStatusCode::available:
        case LocalStatusCode::notAvailable:
        case LocalStatusCode::error:
        case LocalStatusCode::checking:
        {
            qDebug() << "MultiServerUpdatesWidget::checkForUpdates() - forced";
            if (status.state != LocalStatusCode::checking)
                m_updateManager->check();
            m_lastStatus = LocalStatusCode::checking;
            m_checking = true;
            m_updateLocalStateChanged = true;

            // We force update check for all remote peers as well
            auto targets = m_updatesModel->getAllServers();
            auto action = nx::api::Updates2ActionData::ActionCode::check;
            m_updatesTool->requestUpdateAction(action, targets);
            loadDataToUi();
            break;
        }
        default:
            qDebug() << "MultiServerUpdatesWidget::checkForUpdates() - invalid state to issue another check: " << toString(status.state);
            break;
    }
}

void MultiServerUpdatesWidget::at_modelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    for (int row = topLeft.row(); row <= bottomRight.row(); row++)
    {
        QModelIndex index = m_sortedModel->index(row, ServerUpdatesModel::ProgressColumn);
        ui->tableView->openPersistentEditor(index);
    }
}

QString MultiServerUpdatesWidget::toString(LocalStatusCode status)
{
    switch (status)
    {
    case LocalStatusCode::checking:
        return lit("Checking");
    case LocalStatusCode::available:
        return lit("Available");
    case LocalStatusCode::notAvailable:
        return lit("NotAvailable");
    case LocalStatusCode::downloading:
        return lit("Downloading");
    case LocalStatusCode::preparing:
        return lit("Preparing");
    case LocalStatusCode::readyToInstall:
        return lit("ReadyToInstall");
    case LocalStatusCode::installing:
        return lit("Installing");
    case LocalStatusCode::error:
        return lit("Error");
    default:
        NX_ASSERT(false);
        return lit("UnknowState");
    }
}

QString MultiServerUpdatesWidget::toString(WidgetUpdateState state)
{
    switch (state)
    {
    // We have no information about remote state right now.
    case WidgetUpdateState::Initial:
        return lit("Initial");
    // We have obtained some state from the servers. We can do some actions now.
    case WidgetUpdateState::Ready:
        return lit("Ready");
    // We have started legacy update process. Maybe we do not need this
    case WidgetUpdateState::LegacyUpdating:
        return lit("LegacyUpdating");
    // We have issued a command to remote servers to start downloading the updates.
    case WidgetUpdateState::RemoteDownloading:
        return lit("RemoteDownloading");
    // Download update package locally.
    case WidgetUpdateState::LocalDownloading:
        return lit("LocalDownloading");
    // Pushing local update package to server(s).
    case WidgetUpdateState::LocalPushing:
        return lit("LocalPushing");
    // Some servers have downloaded update data and ready to install it.
    case WidgetUpdateState::ReadyInstall:
        return lit("ReadyInstall");
    // Some servers are installing an update.
    case WidgetUpdateState::Installing:
        return lit("Installing");
    default:
        return lit("Unknown");
    }
};

QString MultiServerUpdatesWidget::toString(QnFullUpdateStage stage)
{
    switch (stage)
    {
    case QnFullUpdateStage::Check:
        return tr("Checking for updates...");

    case QnFullUpdateStage::Download:
        return tr("Downloading updates...");

    case QnFullUpdateStage::Client:
        return tr("Installing client update...");

    case QnFullUpdateStage::Incompatible:
        return tr("Installing updates to incompatible servers...");

    case QnFullUpdateStage::Push:
        return tr("Pushing updates to servers...");

    case QnFullUpdateStage::Servers:
        return tr("Installing updates...");

    default:
        return tr("Invalid status...");
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
