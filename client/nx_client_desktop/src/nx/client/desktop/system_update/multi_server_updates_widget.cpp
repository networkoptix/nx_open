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
#include <utils/connection_diagnostics_helper.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/resource_pool_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selection/peer_selector_factory.h>

#include "server_update_tool.h"
#include "server_updates_model.h"
#include "server_status_delegate.h"

using namespace nx::client::desktop::ui;

namespace {

const int kLongInstallWarningTimeoutMs = 2 * 60 * 1000; // 2 minutes
// Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
const quint32 kProcessTerminateTimeoutMs = 15000;

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

namespace nx::vms::common::p2p::downloader {

// This is a hacky class to make P2P downloader work for client as well.
// It should be removed when P2P downloader is properly fixed.
class PeerManagerFactory:
    public AbstractPeerManagerFactory,
    public QnCommonModuleAware
{
    public:
        PeerManagerFactory(QnCommonModule* commonModule):
            QnCommonModuleAware(commonModule)
        {

        }

        virtual AbstractPeerManager* createPeerManager(
            FileInformation::PeerSelectionPolicy peerPolicy,
            const QList<QnUuid>& additionalPeers) override
        {
            auto selector = peer_selection::PeerSelectorFactory::create(peerPolicy, additionalPeers, commonModule());
            return new ResourcePoolPeerManager(commonModule(), std::move(selector), true);
        }
};
} // namespace nx::vms::common::p2p::downloader

namespace nx::client::desktop
{

MultiServerUpdatesWidget::MultiServerUpdatesWidget(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    ui(new Ui::MultiServerUpdatesWidget)
{
    ui->setupUi(this);

    m_updatesTool.reset(new ServerUpdateTool(this));
    m_updatesModel.reset(new ServerUpdatesModel(this));

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
    ui->tableView->setItemDelegateForColumn(ServerUpdatesModel::ProgressColumn, delegate);

    // Per item actions were removed from the spec.
    //connect(delegate, &ServerStatusItemDelegate::updateItemCommand,
    //    this, &MultiServerUpdatesWidget::at_updateItemCommand);

    connect(m_sortedModel.get(), &QnSortedServerUpdatesModel::dataChanged,
        this, &MultiServerUpdatesWidget::at_modelDataChanged);

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

    connect(ui->cancelProgressAction, &QPushButton::clicked, this,
        &MultiServerUpdatesWidget::at_cancelCurrentAction);

    connect(ui->cancelUpdateButton, &QPushButton::clicked, this,
        &MultiServerUpdatesWidget::at_cancelCurrentAction);

    connect(ui->downloadButton, &QPushButton::clicked,
        this, &MultiServerUpdatesWidget::at_startUpdateAction);

    connect(ui->advancedUpdateSettings, &QPushButton::clicked,
        this, [this]()
        {
            auto hidden = ui->tableView->isColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn);
            ui->tableView->setColumnHidden(ServerUpdatesModel::Columns::StorageSettingsColumn, !hidden);
            QString icon = hidden ? lit("text_buttons/expand.png") : lit("text_buttons/collapse.png");
            ui->advancedUpdateSettings->setIcon(qnSkin->icon(icon));
        });

    ui->advancedUpdateSettings->setIcon(qnSkin->icon(lit("text_buttons/collapse.png")));
    /*
    connect(ui->autoCheckUpdates, &QCheckBox::stateChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);
        */

    connect(ui->releaseNotesLabel, &QLabel::linkActivated, this,
        [this]()
        {
            if (m_haveUpdate && !m_updateInfo.releaseNotesUrl.isEmpty())
                QDesktopServices::openUrl(m_updateInfo.releaseNotesUrl);
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

    ui->manualDownloadButton->hide();
    ui->manualDownloadButton->setIcon(qnSkin->icon(lit("text_buttons/download.png")));
    ui->manualDownloadButton->setForegroundRole(QPalette::WindowText);

    initDownloadActions();
    initDropdownActions();

    // Starting timer that tracks remote update state
    m_stateCheckTimer.reset(new QTimer(this));
    m_stateCheckTimer->setSingleShot(false);
    m_stateCheckTimer->start(1000);
    connect(m_stateCheckTimer.get(), &QTimer::timeout,
        this, &MultiServerUpdatesWidget::at_updateCurrentState);

    // Force update when we open dialog.
    checkForInternetUpdates();

    loadDataToUi();
}

MultiServerUpdatesWidget::~MultiServerUpdatesWidget()
{
}

void MultiServerUpdatesWidget::at_updateCurrentState()
{
    NX_ASSERT(m_updatesTool);
    if (isHidden())
        return;

    if (m_updateCheck.valid() && m_updateCheck.wait_for(kWaitForUpdateCheck) == std::future_status::ready)
    {
        auto checkResponse = m_updateCheck.get();
        qDebug() << "Got remote update info: " << checkResponse.info.version;
        m_updateLocalStateChanged = true;
        m_updateInfo = checkResponse.info;
        m_haveUpdate = false;

        m_updateCheckError = "";

        using Error = nx::update::InformationError;
        switch(checkResponse.error)
        {
            case Error::noError:
                m_haveUpdate = true;
                // Everything is good.
                break;
            case Error::networkError:
                // Unable to check update from the internet.
                m_updateCheckError = tr("Unable to check updates on the internet");
                break;
            case Error::httpError:
                // Unable to check update from the internet.
                m_updateCheckError = tr("Unable to check updates on the internet");
                break;
            case Error::jsonError:
                // Broken update server.
            case Error::incompatibleCloudHostError:
                // Incompatible cloud
                m_updateCheckError = tr("Incompatible cloud");
                break;
            case Error::notFoundError:
                // No update
                break;
            default:
                m_updateCheckError = nx::update::toString(checkResponse.error);
                break;
        }
        m_availableVersion = nx::utils::SoftwareVersion(checkResponse.info.version);

        if (m_haveUpdate)
        {
            // Find client package
            const auto modification = QnAppInfo::applicationPlatformModification();
            auto arch = QnAppInfo::applicationArch();
            auto runtime = nx::vms::api::SystemInformation::currentSystemRuntime().replace(L' ', L'_');

            for(const auto& pkg: m_updateInfo.packages)
            {
                if (pkg.component == nx::update::kComponentClient)
                {
                    // Check arch and OS
                    if (pkg.arch == arch && pkg.variant == modification)
                    {
                        m_clientUpdatePackage = pkg;
                        m_haveClientUpdate = true;
                        break;
                    }
                }
            }
        }
        m_updateLocalStateChanged = true;
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
    checkForInternetUpdates();
}

void MultiServerUpdatesWidget::setUpdateSourceMode(UpdateSourceMode mode)
{
    m_updateSourceMode = mode;
    m_updateLocalStateChanged = true;

    loadDataToUi();
}

void MultiServerUpdatesWidget::setModeLocalFile()
{
    auto options = QnCustomFileDialog::fileDialogOptions();
    QString caption = tr("Select Update File...");
    QString filter = tr("Update Files") + " (*.zip)";
    QString fileName = QFileDialog::getOpenFileName(this, caption, QString(), filter, 0, options);

    if (fileName.isEmpty())
        return;

    ui->updateCheckMode->setVisible(false);
    //ui->downloadButton->setText(tr(""));
    m_updateLocalStateChanged = true;
    m_haveClientUpdate = false;
    m_haveUpdate = false;

    m_targetVersion = nx::utils::SoftwareVersion();
    m_updateCheck = m_updatesTool->checkUpdateFromFile(fileName);
    m_updatesModel->setUpdateTarget(nx::utils::SoftwareVersion(), {});
    setUpdateSourceMode(UpdateSourceMode::LocalFile);
}

void MultiServerUpdatesWidget::setModeSpecificBuild()
{
    QnBuildNumberDialog dialog(this);
    if (!dialog.exec())
        return;

    ui->updateCheckMode->setVisible(false);
    nx::utils::SoftwareVersion version = qnStaticCommon->engineVersion();
    m_targetVersion = nx::utils::SoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber());
    m_targetChangeset = dialog.changeset();
    setUpdateSourceMode(UpdateSourceMode::SpecificBuild);
}

void MultiServerUpdatesWidget::setModeLatestAvailable()
{
    m_targetVersion = nx::utils::SoftwareVersion();
    m_targetChangeset = QString();
    ui->updateCheckMode->setVisible(true);
#ifdef TO_BE_REFACTORED
    api::Updates2StatusData status = m_updateManager->status();
#endif
    QString version = versionText(m_availableVersion);
    ui->targetVersionLabel->setText(version);
    setUpdateSourceMode(UpdateSourceMode::LatestVersion);
}

void MultiServerUpdatesWidget::initDropdownActions()
{
    m_selectUpdateTypeMenu.reset(new QMenu(this));
    m_selectUpdateTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceMode::LatestVersion),
        [this]()
        {
            setModeLatestAvailable();
        });

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceMode::SpecificBuild),
        [this]()
        {
            setModeSpecificBuild();
        });

    m_selectUpdateTypeMenu->addAction(toString(UpdateSourceMode::LocalFile),
        [this]()
        {
            setModeLocalFile();
        });

    ui->selectUpdateTypeButton->setMenu(m_selectUpdateTypeMenu.get());
    setModeLatestAvailable();

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
            QSet<QnUuid> targets = m_updatesModel->getAllServers();
            auto url = m_updatesTool->generateUpdatePackageUrl(m_targetVersion, m_targetChangeset, targets, resourcePool()).toString();
            QDesktopServices::openUrl(url);
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            QSet<QnUuid> targets = m_updatesModel->getAllServers();
            auto url = m_updatesTool->generateUpdatePackageUrl(m_targetVersion, m_targetChangeset, targets, resourcePool()).toString();
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

#ifdef TO_BE_REMOVED
// Per item actions were removed from UI spec.
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
            m_updatesTool->requestStopAction({ id });
        }

        // Server was in 'error' state and we decided to retry.
        if (state == StatusCode::error && m_serversFailed.contains(id))
        {
            m_serversFailed.remove(id);
            m_serversActive.insert(id);
            // TODO: Should we really stop it?
            m_updatesTool->requestStartUpdate(m_updateInfo);
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
#endif

void MultiServerUpdatesWidget::at_startUpdateAction()
{
    // Clicked 'Download' button. Should start download process here
    NX_ASSERT(m_updatesTool);

    if (m_haveClientUpdate)
    {
#ifdef TO_BE_REMOVED
        FileInformation info;
        info.md5 = QByteArray::fromHex(m_clientUpdatePackage.md5.toLatin1());
        info.size = m_clientUpdatePackage.size;
        info.name = m_clientUpdatePackage.file;
        info.url = m_clientUpdatePackage.url;
        auto code = m_downloader->addFile(info);
        using Code = vms::common::p2p::downloader::ResultCode;

        switch(code)
        {
            case Code::ok:
                qDebug() << "MultiServerUpdatesWidget::at_startUpdateAction() - downloading client package"
                         << info.name;
                break;
            case Code::fileAlreadyExists:
                qDebug() << "MultiServerUpdatesWidget::at_startUpdateAction() - file is already here"
                         << info.name;
                break;
            default:
                // TODO: Some sort of an error here
                qDebug() << "MultiServerUpdatesWidget::at_startUpdateAction() - failed to add client package "
                         << info.name
                         << vms::common::p2p::downloader::toString(code);
                break;
        }
#endif
    }

    if (m_updateStateCurrent == WidgetUpdateState::ReadyInstall)
    {
        auto targets = m_updatesModel->getServersInState(StatusCode::readyToInstall);
        if (targets.empty())
        {
            qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - no server can install anything";
            return;
        }

        qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - starting installation";
        moveTowardsState(WidgetUpdateState::Installing, targets);
        m_updatesTool->requestInstallAction(targets);
    }
    else if (m_updateStateCurrent == WidgetUpdateState::Ready && m_haveUpdate)
    {
        if (m_updateSourceMode == UpdateSourceMode::LocalFile)
        {
            m_updatesTool->startUpload();
            moveTowardsState(WidgetUpdateState::Pushing, {});
        }
        else
        {
            // Start download process
            // Move to state 'Downloading'
            // At this state should check remote state untill everything is complete

            /*
            auto targets = m_updatesModel->getServersInState(StatusCode::available);
            if (targets.empty())
            {
                qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - no servers can download update";
                return;
            }*/
            auto targets = m_updatesModel->getAllServers();
            qDebug() << "MultiServerUpdatesWidget::at_downloadUpdate() - sending 'download' command";
            auto offlineServers = m_updatesModel->getOfflineServers();
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
            }

            auto incompatible = m_updatesModel->getLegacyServers();
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
            }

            moveTowardsState(WidgetUpdateState::RemoteDownloading, targets);
            m_updatesTool->requestStartUpdate(m_updateInfo);
        }
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
        m_updatesTool->requestStopAction(serversToCancel);
        moveTowardsState(WidgetUpdateState::Ready, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::Installing)
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent);
        // Should send 'cancel' command to all the?
        auto serversToCancel = m_updatesModel->getServersInState(StatusCode::installing);
        m_updatesTool->requestStopAction(serversToCancel);
        moveTowardsState(WidgetUpdateState::Initial, {});
    }
    else if (m_updateStateCurrent == WidgetUpdateState::ReadyInstall)
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent);
        auto serversToCancel = m_updatesModel->getServersInState(StatusCode::readyToInstall);
        m_updatesTool->requestStopAction(serversToCancel);
        moveTowardsState(WidgetUpdateState::Initial, {});
    }
    if (m_updateStateCurrent == WidgetUpdateState::Pushing)
    {
        m_updatesTool->stopUpload();
        moveTowardsState(WidgetUpdateState::Ready, {});
    }
    else
    {
        qDebug() << "MultiServerUpdatesWidget::at_cancelCurrentAction() at" << toString(m_updateStateCurrent) << ": not implemented";
    }
    // Spec says that we can not cancel anything when we began installing stuff
    return true;
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

bool MultiServerUpdatesWidget::isChecking() const
{
    return m_updateCheck.valid();
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
        qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - we are in initial state and finally got status for remote system";
        moveTowardsState(WidgetUpdateState::Ready, {});
    }
}

void MultiServerUpdatesWidget::processRemoteDownloading(
    const ServerUpdateTool::RemoteStatus& remoteStatus)
{
    for (const auto& status : remoteStatus)
    {
        auto id = status.first;

        switch (status.second.code)
        {
        case StatusCode::readyToInstall:
            qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - server " << id << "completed downloading and is ready to install";
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
                m_updatesTool->requestInstallAction(serversToUpdate);
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
                //m_updatesTool->requestStartUpdate(serversToRetry, m_updateInfo);
                m_updatesTool->requestStartUpdate(m_updateInfo);
                moveTowardsState(WidgetUpdateState::RemoteDownloading, serversToRetry);
            }
            else if (clicked == cancelUpdate)
            {
                auto serversToStop = m_serversIssued;
                m_updatesTool->requestStopAction(m_serversIssued);
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
                m_updatesTool->requestInstallAction(serversToRetry);
                moveTowardsState(WidgetUpdateState::ReadyInstall, serversToRetry);
            }
            else if (clicked == skipFailed)
            {
                // Start installing process for servers that have succeded downloading
                auto serversToIntall = m_serversIssued;
                m_updatesTool->requestInstallAction(serversToIntall);
                moveTowardsState(WidgetUpdateState::Installing, serversToIntall);
            }
            else if (clicked == cancelUpdate)
            {
                m_updatesTool->requestStopAction(m_serversIssued);
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
    const ServerUpdateTool::RemoteStatus& remoteStatus)
{
    // TODO: Implement it
    for (const auto& status : remoteStatus)
    {
        auto id = status.first;
        auto state = status.second.code;

        switch (state)
        {
        case StatusCode::preparing:
        case StatusCode::installing:
            // Nothing to do here. We just wait until it completes
            break;
        case StatusCode::readyToInstall:
            if (!m_serversComplete.contains(id))
            {
                qDebug() << "MultiServerUpdatesWidget::processRemoteChanges() - server " << id << "is ready to install. Should be in 'installing' phase";
                m_serversComplete.insert(id);
            }
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

    ServerUpdateTool::RemoteStatus remoteStatus;
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

void MultiServerUpdatesWidget::moveTowardsState(WidgetUpdateState state, QSet<QnUuid> targets)
{
    if (state != m_updateStateCurrent)
    {
        qDebug() << "MultiServerUpdatesWidget::moveTowardsState changing state from "
                 << toString(m_updateStateCurrent)
                 << "to"
                 << toString(state);
        switch (state)
        {
            case WidgetUpdateState::Initial:
                break;
            case WidgetUpdateState::Ready:
                break;
            case WidgetUpdateState::RemoteDownloading:
                break;
            case WidgetUpdateState::LocalDownloading:
                break;
            case WidgetUpdateState::Pushing:
                break;
            case WidgetUpdateState::ReadyInstall:
                break;
            case WidgetUpdateState::Installing:
                break;
            default:
                break;
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
}

void MultiServerUpdatesWidget::syncUpdateSource()
{
    // Code from the former 'updateAccent'
    // 'Update' button accented by default if update is possible
    //bool accented = m_lastUpdateCheckResult.result == QnCheckForUpdateResult::UpdateFound;
    bool hasLatestVersion = m_updateSourceMode == UpdateSourceMode::LatestVersion
        && (m_availableVersion.isNull() || m_updatesModel->lowestInstalledVersion() >= m_availableVersion) && !m_updateCheckError.isEmpty();

    if (isChecking())
    {
        ui->manualDownloadButton->hide();
        ui->updateStackedWidget->hide();
        ui->versionStackedWidget->setCurrentWidget(ui->checkingPage);
        ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
    }
    else
    {
        ui->manualDownloadButton->show();
        ui->updateStackedWidget->show();
        ui->versionStackedWidget->setCurrentWidget(ui->versionPage);
        ui->releaseNotesLabel->setVisible(!m_updateInfo.releaseNotesUrl.isEmpty());

        if (!m_updateCheckError.isEmpty())
        {
            ui->errorLabel->setText(m_updateCheckError);
            ui->infoStackedWidget->setCurrentWidget(ui->errorPage);
            ui->downloadButton->setVisible(false);
        }
        else if (hasLatestVersion)
        {
            ui->versionStackedWidget->setCurrentWidget(ui->latestVersionPage);
            ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
        }
        else
        {
            ui->downloadButton->setVisible(true);
            if (!m_updateInfo.releaseNotesUrl.isEmpty())
            {
                ui->infoStackedWidget->setCurrentWidget(ui->releaseNotesPage);
            }
            else
            {
                ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
            }
        }

        ui->latestVersionIconLabel->setVisible(hasLatestVersion);
        m_updatesModel->setUpdateTarget(m_availableVersion, {});

        if (m_updateStateCurrent == WidgetUpdateState::Ready)
        {

        }
    }

    //int eulaVersion = QnClientAppInfo::eulaVersion();
    //ui->eulaInfo->setVisible(m_haveUpdate && eulaVersion != m_updateInfo.eulaVersion);

    ui->selectUpdateTypeButton->setText(toString(m_updateSourceMode));

    if (m_latestVersionChanged)
    {
        bool showButton = m_updateSourceMode != UpdateSourceMode::LocalFile
            && !hasLatestVersion;

        ui->manualDownloadButton->setVisible(showButton);

        m_latestVersionChanged = false;
    }

    ui->targetVersionLabel->setText(versionText(m_availableVersion));

    m_updateLocalStateChanged = false;

    ui->selectUpdateTypeButton->setEnabled(!isChecking());
}

void MultiServerUpdatesWidget::syncRemoteUpdateState()
{
    // This function gathers state status of update from remote servers and changes
    // UI state accordingly.

    bool hideInfo = m_updateStateCurrent == WidgetUpdateState::Initial
        || m_updateStateCurrent == WidgetUpdateState::Ready;
    hideStatusColumns(hideInfo);

    // Should we calculate progress for this UI state
    bool hasProgress = false;
    // Title to be shown for this UI state
    QString updateTitle;

    ui->cancelUpdateButton->setVisible(false);
    ui->advancedUpdateSettings->setVisible(false);
    switch (m_updateStateCurrent)
    {
        case WidgetUpdateState::Initial:
            break;

        case WidgetUpdateState::Ready:
            if(m_haveUpdate)
            {
                if (m_updateSourceMode == UpdateSourceMode::LocalFile)
                {
                    ui->downloadButton->setText(tr("Upload"));
                    ui->downloadAndInstall->setText(tr("Upload && Install"));
                    ui->advancedUpdateSettings->setVisible(true);
                }
                else // LatestVersion or SpecificBuild)
                {
                    ui->downloadButton->setText(tr("Download"));
                    ui->downloadAndInstall->setText(tr("Download && Install"));
                    ui->advancedUpdateSettings->setVisible(true);
                }
            }
            else
            {
                if (m_updateSourceMode == UpdateSourceMode::SpecificBuild)
                {
                    ui->browseUpdate->setVisible(true);
                    ui->browseUpdate->setText(tr("Select Another Build"));
                }
                else if (m_updateSourceMode == UpdateSourceMode::LocalFile)
                {
                    ui->browseUpdate->setVisible(true);
                    ui->browseUpdate->setText(tr("Browse for Another File..."));
                }
                else // LatestVersion
                {
                    ui->browseUpdate->setVisible(false);
                }
            }
            break;
        case WidgetUpdateState::LocalDownloading:
            updateTitle = tr("Updating to ...");
            hasProgress = true;
            break;
        case WidgetUpdateState::RemoteDownloading:
            updateTitle = tr("Updating to ...");
            hasProgress = true;
            ui->cancelUpdateButton->setVisible(true);
            break;
        case WidgetUpdateState::ReadyInstall:
            updateTitle = tr("Ready to update to");
            ui->downloadButton->setText(tr("Install update"));
            ui->downloadAndInstall->setVisible(false);
            ui->advancedUpdateSettings->setVisible(true);
            break;
        case WidgetUpdateState::Installing:
            updateTitle = tr("Updating to ...");
            hasProgress = true;
            break;
        case WidgetUpdateState::Pushing:
            ui->cancelUpdateButton->setVisible(true);
            break;
        default:
            break;
    }

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

    if (hasProgress)
    {
        ProgressInfo info = calculateActionProgress();
        ui->actionProgess->setValue(info.current);
        ui->actionProgess->setMaximum(info.max);
        ui->updateStackedWidget->setCurrentWidget(ui->updateProgressPage);
    }
    else
    {
        ui->updateStackedWidget->setCurrentWidget(ui->updateControlsPage);
    }
    //qDebug() << "MultiServerUpdatesWidget::loadDataToUi - done with m_updateStateChanged";
    m_updateRemoteStateChanged = false;
}

void MultiServerUpdatesWidget::loadDataToUi()
{
    // Synchronises internal state and UI widget state.
    NX_ASSERT(m_updatesTool);

    processRemoteChanges();

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

    bool endOfTheWeek = QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek;
    ui->dayWarningBanner->setVisible(endOfTheWeek);

    // TODO: Update logic for auto check
    //setAutoUpdateCheckMode(qnGlobalSettings->isUpdateNotificationsEnabled());

    QString debugState;
    debugState += lit("Widget=")+toString(m_updateStateCurrent);
    ui->debugStateLabel->setText(debugState);
}

void MultiServerUpdatesWidget::applyChanges()
{
    // TODO: Update logic for auto check
    qnGlobalSettings->setUpdateNotificationsEnabled(m_autoCheckMenu);
    qnGlobalSettings->synchronizeNow();
}

void MultiServerUpdatesWidget::discardChanges()
{
    //
    //if (!m_updatesTool->isUpdating())
    //    return;
    return;

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

    //return qnGlobalSettings->isUpdateNotificationsEnabled() != ui->autoCheckUpdates->isChecked();
    return false;
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

void MultiServerUpdatesWidget::autoCheckForUpdates()
{
    if (m_updateSourceMode != UpdateSourceMode::LatestVersion)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAutoUpdateCheck < kAutoCheckIntervalMs)
        return;

    checkForInternetUpdates();
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

void MultiServerUpdatesWidget::checkForInternetUpdates()
{
    if (!m_updateCheck.valid())
    {
        m_updateLocalStateChanged = true;
        m_haveClientUpdate = false;
        m_haveUpdate = false;

        QString updateUrl = qnSettings->updateFeedUrl();
        m_updateCheck = std::async(std::launch::async,
            [updateUrl]()
            {
                return ServerUpdateTool::checkUpdateFromInternet(updateUrl);
            });
        syncUpdateSource();
    }
}

void MultiServerUpdatesWidget::at_modelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& /*unused*/)
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
        /*
        case LocalStatusCode::checking:
            return "Checking";
        case LocalStatusCode::available:
            return "Available";
        case LocalStatusCode::notAvailable:
            return "NotAvailable";*/
        case LocalStatusCode::downloading:
            return "Downloading";
        case LocalStatusCode::preparing:
            return "Preparing";
        case LocalStatusCode::readyToInstall:
            return "ReadyToInstall";
        case LocalStatusCode::installing:
            return "Installing";
        case LocalStatusCode::error:
            return "Error";
        default:
            NX_ASSERT(false);
            return "UnknowState";
    }
}

QString MultiServerUpdatesWidget::toString(WidgetUpdateState state)
{
    switch (state)
    {
        // We have no information about remote state right now.
        case WidgetUpdateState::Initial:
            return "Initial";
        // We have obtained some state from the servers. We can do some actions now.
        case WidgetUpdateState::Ready:
            return "Ready";
        // We have issued a command to remote servers to start downloading the updates.
        case WidgetUpdateState::RemoteDownloading:
            return "RemoteDownloading";
        // Download update package locally.
        case WidgetUpdateState::LocalDownloading:
            return "LocalDownloading";
        // Pushing local update package to server(s).
        case WidgetUpdateState::Pushing:
            return "LocalPushing";
        // Some servers have downloaded update data and ready to install it.
        case WidgetUpdateState::ReadyInstall:
            return "ReadyInstall";
        // Some servers are installing an update.
        case WidgetUpdateState::Installing:
            return "Installing";
        default:
            return "Unknown";
    }
}

QString MultiServerUpdatesWidget::toString(UpdateSourceMode mode)
{
    switch(mode)
    {
        case UpdateSourceMode::LatestVersion:
            return tr("Available Update");
        case UpdateSourceMode::SpecificBuild:
            return tr("Specific Build...");
        case UpdateSourceMode::LocalFile:
            return tr("Browse for Update File...");
    }
    return "Unknown update source mode";
}

} // namespace nx::client::desktop
