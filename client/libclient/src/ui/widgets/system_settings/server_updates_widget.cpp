#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <ui/common/palette.h>
#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/actions/action_manager.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench_access_controller.h>

#include <update/media_server_update_tool.h>
#include <update/low_free_space_warning.h>

#include <utils/applauncher_utils.h>
#include <utils/common/app_info.h>
#include <utils/common/scoped_value_rollback.h>

namespace {

    const int kLongInstallWarningTimeoutMs = 2 * 60 * 1000; // 2 minutes
    // Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
    const quint32 kProcessTerminateTimeoutMs = 15000;

    const int kTooLateDayOfWeek = Qt::Thursday;

    const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour

    const int kMaxLabelWidth = 400; // pixels

    const int kVersionLabelFontSizePixels = 24;

    const int kVersionLabelFontWeight = QFont::DemiBold;

    const int kLinkCopiedMessageTimeoutMs = 2000;

    /* N-dash 5 times: */
    const QString kNoVersionNumberText = QString::fromWCharArray(L"\x2013\x2013\x2013\x2013\x2013");

    QString elidedText(const QString& text, const QFontMetrics& fontMetrics)
    {
        QString result;

        for (const auto& line: text.split(L'\n', QString::KeepEmptyParts))
        {
            if (result.isEmpty())
                result += L'\n';

            result += fontMetrics.elidedText(line, Qt::ElideMiddle, kMaxLabelWidth);
        }

        return result;
    }

} // anonymous namespace

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    ui(new Ui::QnServerUpdatesWidget),
    m_latestVersion(),
    m_checking(false),
    m_longUpdateWarningTimer(new QTimer(this)),
    m_lastAutoUpdateCheck(0)
{
    ui->setupUi(this);

    QFont font;
    font.setPixelSize(kVersionLabelFontSizePixels);
    font.setWeight(kVersionLabelFontWeight);
    ui->targetVersionLabel->setFont(font);
    ui->targetVersionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->targetVersionLabel->setForegroundRole(QPalette::Text);

    ui->linkCopiedIconLabel->setPixmap(qnSkin->pixmap("buttons/checkmark.png"));
    ui->linkCopiedWidget->hide();

    setHelpTopic(this, Qn::Administration_Update_Help);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(m_updateTool, this);

    QnSortedServerUpdatesModel* sortedUpdatesModel = new QnSortedServerUpdatesModel(this);
    sortedUpdatesModel->setSourceModel(m_updatesModel);
    sortedUpdatesModel->sort(0); // the column does not matter because the model uses column-independent sorting

    ui->tableView->setModel(sortedUpdatesModel);
    ui->tableView->setItemDelegateForColumn(QnServerUpdatesModel::VersionColumn, new QnUpdateStatusItemDelegate(ui->tableView));

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionsClickable(false);

    ui->refreshButton->setIcon(qnSkin->icon("buttons/refresh.png"));
    ui->updateButton->setEnabled(false);

    connect(ui->cancelButton, &QPushButton::clicked, this,
        [this]()
        {

            if (accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
            {
                ui->cancelButton->setEnabled(false);
                m_updateTool->cancelUpdate();
                ui->cancelButton->setEnabled(true);
            }
        });

    connect(ui->updateButton, &QPushButton::clicked, this, [this]()
    {
        if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
            return;

        if (m_localFileName.isEmpty())
            m_updateTool->startUpdate(m_targetVersion);
        else
            m_updateTool->startUpdate(m_localFileName);
    });

    connect(ui->refreshButton, &QPushButton::clicked, this, &QnServerUpdatesWidget::refresh);

    connect(ui->updatesNotificationCheckbox, &QCheckBox::stateChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->releaseNotesLabel, &QLabel::linkActivated, this, [this]()
    {
        if (!m_releaseNotesUrl.isEmpty())
            QDesktopServices::openUrl(m_releaseNotesUrl);
    });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
        this, &QnServerUpdatesWidget::refresh);

    connect(m_updateTool, &QnMediaServerUpdateTool::stageChanged,
        this, &QnServerUpdatesWidget::at_tool_stageChanged);
    connect(m_updateTool, &QnMediaServerUpdateTool::stageProgressChanged,
        this, &QnServerUpdatesWidget::at_tool_stageProgressChanged);
    connect(m_updateTool, &QnMediaServerUpdateTool::updateFinished,
        this, &QnServerUpdatesWidget::at_updateFinished);
    connect(m_updateTool, &QnMediaServerUpdateTool::lowFreeSpaceWarning,
        this, &QnServerUpdatesWidget::at_tool_lowFreeSpaceWarning, Qt::DirectConnection);
    connect(m_updateTool, &QnMediaServerUpdateTool::updatesCheckCanceled,
        this, &QnServerUpdatesWidget::at_tool_updatesCheckCanceled);

    setWarningStyle(ui->errorLabel);
    setWarningStyle(ui->longUpdateWarning);

    ui->infoStackedWidget->setCurrentWidget(ui->errorPage);
    ui->errorLabel->setText(QString());

    ui->dayWarningBanner->setBackgroundRole(QPalette::AlternateBase);
    ui->dayWarningLabel->setForegroundRole(QPalette::Text);

    static_assert(kTooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningBanner->setVisible(false);
    ui->longUpdateWarning->setVisible(false);

    ui->releaseNotesLabel->setText(lit("<a href='notes'>%1</a>").arg(tr("Release notes")));

    QTimer* updateTimer = new QTimer(this);
    updateTimer->setSingleShot(false);
    updateTimer->start(kAutoCheckIntervalMs);
    connect(updateTimer, &QTimer::timeout, this, &QnServerUpdatesWidget::autoCheckForUpdates);

    m_longUpdateWarningTimer->setInterval(kLongInstallWarningTimeoutMs);
    m_longUpdateWarningTimer->setSingleShot(true);
    connect(m_longUpdateWarningTimer, &QTimer::timeout, ui->longUpdateWarning, &QLabel::show);

    connect(qnGlobalSettings, &QnGlobalSettings::updateNotificationsChanged,
        this, &QnAbstractPreferencesWidget::loadDataToUi);

    at_tool_stageChanged(QnFullUpdateStage::Init);

    ui->downloadButton->hide();
    ui->downloadButton->setIcon(qnSkin->icon(lit("buttons/download.png")));
    ui->downloadButton->setForegroundRole(QPalette::WindowText);
    initDownloadActions();

    initDropdownActions();
}

bool QnServerUpdatesWidget::tryClose(bool /*force*/)
{
    m_updateTool->cancelUpdatesCheck();
    return true;
}

void QnServerUpdatesWidget::forcedUpdate()
{
    refresh();
}

void QnServerUpdatesWidget::initDropdownActions()
{
    auto selectUpdateTypeMenu = new QMenu(this);
    selectUpdateTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    auto defaultAction = selectUpdateTypeMenu->addAction(tr("Latest Available Update"),
        [this]()
        {
            m_targetVersion = QnSoftwareVersion();
            m_localFileName = QString();

            ui->selectUpdateTypeButton->setText(tr("Latest Available Update"));
            ui->targetVersionLabel->setText(m_latestVersion.isNull()
                ? kNoVersionNumberText
                : m_latestVersion.toString());

            ui->downloadButton->setText(tr("Download the Latest Version Update File"));
            ui->downloadButton->show();

            checkForUpdates(true);
        });

    selectUpdateTypeMenu->addAction(tr("Select Specific Build..."),
        [this]()
        {
            QnBuildNumberDialog dialog(this);
            if (!dialog.exec())
                return;

            QnSoftwareVersion version = qnCommon->engineVersion();
            m_targetVersion = QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber());
            m_localFileName = QString();

            ui->targetVersionLabel->setText(m_targetVersion.toString());
            ui->selectUpdateTypeButton->setText(tr("Selected Version"));

            ui->downloadButton->setText(tr("Download Update File"));
            ui->downloadButton->hide();

            checkForUpdates(true);
        });

    selectUpdateTypeMenu->addAction(tr("Browse for Update File..."),
        [this]()
        {
            m_localFileName = QnFileDialog::getOpenFileName(this,
                tr("Select Update File..."),
                QString(), tr("Update Files (*.zip)"),
                0,
                QnCustomFileDialog::fileDialogOptions());

            if (m_localFileName.isEmpty())
                return;

            ui->targetVersionLabel->setText(kNoVersionNumberText);
            ui->selectUpdateTypeButton->setText(tr("Selected Update File"));
            ui->downloadButton->hide();

            checkForUpdates(false);
        });

    ui->selectUpdateTypeButton->setMenu(selectUpdateTypeMenu);

    defaultAction->trigger();
}

void QnServerUpdatesWidget::initDownloadActions()
{
    auto downloadLinkMenu = new QMenu(this);
    downloadLinkMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    downloadLinkMenu->addAction(tr("Download in External Browser"),
        [this]()
        {
            QDesktopServices::openUrl(
                m_updateTool->generateUpdatePackageUrl(m_targetVersion));
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            qApp->clipboard()->setText(
                m_updateTool->generateUpdatePackageUrl(m_targetVersion).toString());

            ui->linkCopiedWidget->show();
            fadeWidget(ui->linkCopiedWidget, 1.0, 0.0, kLinkCopiedMessageTimeoutMs, 1.0,
                [this]()
                {
                    ui->linkCopiedWidget->setGraphicsEffect(nullptr);
                    ui->linkCopiedWidget->hide();
                });
        });

    connect(ui->downloadButton, &QPushButton::clicked, this,
        [this, downloadLinkMenu]()
        {
            downloadLinkMenu->exec(ui->downloadButton->mapToGlobal(
                ui->downloadButton->rect().bottomLeft() + QPoint(0, 1)));

            ui->downloadButton->update();
        });
}

bool QnServerUpdatesWidget::cancelUpdate()
 {
    if (m_updateTool->isUpdating())
        return m_updateTool->cancelUpdate();

    return true;
}

bool QnServerUpdatesWidget::canCancelUpdate() const
{
    if (m_updateTool->isUpdating())
        return m_updateTool->canCancelUpdate();

    return true;
}

bool QnServerUpdatesWidget::isUpdating() const
{
    return m_updateTool->isUpdating();
}

QnMediaServerUpdateTool* QnServerUpdatesWidget::updateTool() const
{
    return m_updateTool;
}

void QnServerUpdatesWidget::loadDataToUi()
{
    ui->updatesNotificationCheckbox->setChecked(qnGlobalSettings->isUpdateNotificationsEnabled());
}

void QnServerUpdatesWidget::applyChanges()
{
    qnGlobalSettings->setUpdateNotificationsEnabled(ui->updatesNotificationCheckbox->isChecked());
    qnGlobalSettings->synchronizeNow();
}

void QnServerUpdatesWidget::discardChanges()
{
    if (!canCancelUpdate())
        QnMessageBox::critical(this, tr("Error"), tr("Cannot cancel update at this state.") + L'\n' + tr("Please wait until update is finished"));
    else
        cancelUpdate();
}

bool QnServerUpdatesWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return qnGlobalSettings->isUpdateNotificationsEnabled() != ui->updatesNotificationCheckbox->isChecked();
}

bool QnServerUpdatesWidget::canApplyChanges() const
{
    //TODO: #GDM now this prevents other tabs from saving their changes
    if (isUpdating())
        return false;
    return true;
}

bool QnServerUpdatesWidget::canDiscardChanges() const
{
    //TODO: #GDM now this prevents other tabs from discarding their changes
    if (!canCancelUpdate())
        return false;

    return true;
}

void QnServerUpdatesWidget::autoCheckForUpdates()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAutoUpdateCheck < kAutoCheckIntervalMs)
        return;

    /* Do not check while updating. */
    if (!m_updateTool->idle())
        return;

    /* Do not recheck specific build. */
    if (!m_targetVersion.isNull())
        return;

    /* Do not check if local file is selected. */
    if (!m_localFileName.isEmpty())
        return;

    checkForUpdates(true);
}

bool QnServerUpdatesWidget::beginChecking()
{
    if (m_checking || !m_updateTool->idle())
        return false;

    m_checking = true;
    ui->updateButton->setEnabled(false);
    ui->selectUpdateTypeButton->setEnabled(false);
    ui->targetVersionLabel->setPalette(palette());
    ui->versionStackedWidget->setCurrentWidget(ui->checkingPage);
    ui->errorLabel->setText(QString());

    return true;
}

void QnServerUpdatesWidget::endChecking(const QnCheckForUpdateResult& result)
{
    if (!m_checking)
        return;

    m_checking = false;

    m_lastUpdateCheckResult = result;

    ui->selectUpdateTypeButton->setEnabled(true);
    ui->updateButton->setEnabled(
        result.result == QnCheckForUpdateResult::UpdateFound
        && m_updateTool->idle());

    ui->downloadButton->setEnabled(true);

    m_updatesModel->setCheckResult(result);

    QnSoftwareVersion displayVersion = result.version.isNull()
        ? m_latestVersion
        : result.version;

    QString detail;
    auto versionText = displayVersion.isNull() ? kNoVersionNumberText : displayVersion.toString();

    setWarningStyle(ui->errorLabel);

    switch (result.result)
    {
        case QnCheckForUpdateResult::UpdateFound:
        {
            if (!result.version.isNull() && result.clientInstallerRequired)
                detail = tr("You will have to update the client manually using an installer.");
            break;
        }

        case QnCheckForUpdateResult::NoNewerVersion:
            setPaletteColor(ui->errorLabel, QPalette::WindowText, qnGlobals->successTextColor());
            detail = m_targetVersion.isNull()
                ? tr("All components in your system are up to date.")
                : tr("All components in your system are up to this version.");
            break;

        case QnCheckForUpdateResult::InternetProblem:
            versionText = kNoVersionNumberText;
            detail = tr("Unable to check updates on the Internet.");
            break;

        case QnCheckForUpdateResult::NoSuchBuild:
            detail = tr("Unknown build number.");
            setWarningStyle(ui->targetVersionLabel);
            ui->downloadButton->setEnabled(false);
            break;

        case QnCheckForUpdateResult::DowngradeIsProhibited:
            detail = tr("Downgrade to an earlier version is prohibited.");
            setWarningStyle(ui->targetVersionLabel);
            break;

        case QnCheckForUpdateResult::BadUpdateFile:
            versionText = kNoVersionNumberText;
            detail = tr("Cannot update from this file.");
            break;

        case QnCheckForUpdateResult::ServerUpdateImpossible:
            detail = tr("Updates for one or more servers were not found.");
            break;

        case QnCheckForUpdateResult::ClientUpdateImpossible:
            detail = tr("Client update was not found.");
            break;

        case QnCheckForUpdateResult::NoFreeSpace:
            detail = tr("Unable to extract update file. No free space left on the disk.");
            break;

        case QnCheckForUpdateResult::IncompatibleCloudHost:
            detail = tr("Incompatible %1 instance. To update disconnect system from %1 first.",
                "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
                .arg(QnAppInfo::cloudName());
            break;

        default:
            NX_ASSERT(false); // should never get here
    }

    ui->errorLabel->setText(detail);
    ui->infoStackedWidget->setCurrentWidget(detail.isEmpty() ? ui->releaseNotesPage : ui->errorPage);

    m_releaseNotesUrl = result.releaseNotesUrl;
    ui->releaseNotesLabel->setVisible(!m_releaseNotesUrl.isEmpty());

    ui->targetVersionLabel->setText(versionText);

    ui->versionStackedWidget->setCurrentWidget(ui->versionPage);
}

bool QnServerUpdatesWidget::restartClient(const QnSoftwareVersion& version)
{
    if (!applauncher::checkOnline())
        return false;

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

void QnServerUpdatesWidget::checkForUpdates(bool fromInternet)
{
    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return;

    if (!beginChecking())
        return;

    if (fromInternet)
        m_lastAutoUpdateCheck = QDateTime::currentMSecsSinceEpoch();

    ui->refreshButton->hide();
    ui->targetVersionLabel->setPalette(palette());

    if (fromInternet)
    {
        m_updateTool->checkForUpdates(m_targetVersion,
            [this](const QnCheckForUpdateResult& result)
            {
                /* Update latest version if we have checked for updates in the internet. */
                if (m_targetVersion.isNull() && !result.version.isNull())
                {
                    m_latestVersion = result.version;
                    m_updatesModel->setLatestVersion(m_latestVersion);
                }

                ui->refreshButton->show();
                ui->downloadButton->show();
                endChecking(result);
            });
    }
    else
    {
        m_updateTool->checkForUpdates(m_localFileName,
            [this](const QnCheckForUpdateResult& result)
            {
                endChecking(result);
            });
    }
}

void QnServerUpdatesWidget::refresh()
{
    checkForUpdates(m_localFileName.isEmpty());
}

void QnServerUpdatesWidget::at_tool_stageChanged(QnFullUpdateStage stage)
{
    if (stage != QnFullUpdateStage::Init)
    {
        ui->updateButton->setEnabled(false);
        ui->titleStackedWidget->setCurrentWidget(ui->updatingPage);
        ui->updateStackedWidget->setCurrentWidget(ui->updateProgressPage);
    }
    else
    { /* Stage returned to idle, update finished. */
        ui->longUpdateWarning->hide();
        ui->titleStackedWidget->setCurrentWidget(ui->selectUpdateTypePage);
        ui->updateStackedWidget->setCurrentWidget(ui->updateControlsPage);
        m_longUpdateWarningTimer->stop();
    }

    ui->dayWarningBanner->setVisible(QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek);

    bool cancellable = false;
    switch (stage)
    {
        case QnFullUpdateStage::Check:
        case QnFullUpdateStage::Download:
        case QnFullUpdateStage::Client:
        case QnFullUpdateStage::Incompatible:
        case QnFullUpdateStage::CheckFreeSpace:
        case QnFullUpdateStage::Push:
            cancellable = true;
            break;

        case QnFullUpdateStage::Servers:
            m_longUpdateWarningTimer->start();
            break;

        default:
            break;
    }

    ui->cancelButton->setEnabled(cancellable);
}

void QnServerUpdatesWidget::at_tool_stageProgressChanged(QnFullUpdateStage stage, int progress)
{

    int offset = 0;
    switch (stage)
    {
        case QnFullUpdateStage::Client:
            offset = 50;
            break;

        case QnFullUpdateStage::Incompatible:
            offset = 50;
            break;

        default:
            break;
    }

    int displayStage = qMax(static_cast<int>(stage) - 1, 0);
    int value = (displayStage*100 + offset + progress) / (static_cast<int>(QnFullUpdateStage::Count) - 1);

    QString status;
    switch (stage)
    {
        case QnFullUpdateStage::Check:
            status = tr("Checking for updates...");
            break;

        case QnFullUpdateStage::Download:
            status = tr("Downloading updates...");
            break;

        case QnFullUpdateStage::Client:
            status = tr("Installing client update...");
            break;

        case QnFullUpdateStage::Incompatible:
            status = tr("Installing updates to incompatible servers...");
            break;

        case QnFullUpdateStage::Push:
            status = tr("Pushing updates to servers...");
            break;

        case QnFullUpdateStage::Servers:
            status = tr("Installing updates...");
            break;

        default:
            break;
    }

    static const QString kPercentSuffix = lit("\t%1%");
    status += kPercentSuffix.arg(value);

    ui->updateProgess->setValue(value);
    ui->updateProgess->setFormat(status);
}

void QnServerUpdatesWidget::at_tool_lowFreeSpaceWarning(QnLowFreeSpaceWarning& lowFreeSpaceWarning)
{
    const auto failedServers =
        qnResPool->getResources<QnMediaServerResource>(lowFreeSpaceWarning.failedPeers);

    QScopedPointer<QnMessageBox> dialog(new QnMessageBox(
        QnMessageBox::Warning, -1, tr("Warning"),
        tr("Not enough free space at %n servers:", "", failedServers.size())
            + lit("\n") + serverNamesString(failedServers)
            + lit("\n") + tr("Do you want to continue?"),
        QDialogButtonBox::Cancel,
        this));

    dialog->addButton(tr("Force pushing updates"), QDialogButtonBox::AcceptRole);
    dialog->setDefaultButton(QDialogButtonBox::Cancel);

    const auto result = dialog->exec();

    lowFreeSpaceWarning.ignore = true;

    if (result == QDialogButtonBox::Cancel)
        m_updateTool->cancelUpdate();
}

void QnServerUpdatesWidget::at_tool_updatesCheckCanceled()
{
    endChecking(m_lastUpdateCheckResult);
}

QString QnServerUpdatesWidget::serverNamesString(const QnMediaServerResourceList& servers)
{
    QString result;

    for (const QnMediaServerResourcePtr& server : servers)
    {
        if (!result.isEmpty())
            result += lit("\n");

        result += QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());
    }

    return elidedText(result, fontMetrics());
}

void QnServerUpdatesWidget::at_updateFinished(const QnUpdateResult& result)
{
    ui->updateProgess->setValue(100);
    ui->updateProgess->setFormat(tr("Update Finished...") + lit("\t100%"));

    if (isVisible())
    {
        switch (result.result)
        {
            case QnUpdateResult::Successful:
            {
                QString message = result.errorMessage();

                bool clientUpdated = (result.targetVersion != qnCommon->engineVersion());
                if (clientUpdated)
                {
                    message += lit("\n");
                    if (result.clientInstallerRequired)
                    {
                        message += tr("Please update the client manually using an installation package.");
                    }
                    else
                    {
                        message += tr("The client will be restarted to the updated version.");
                    }
                }

                QnMessageBox::information(this, tr("Update Succeeded"), message);

                bool unholdConnection = !clientUpdated || result.clientInstallerRequired || result.protocolChanged;
                if (clientUpdated && !result.clientInstallerRequired)
                {
                    if (!restartClient(result.targetVersion))
                    {
                        unholdConnection = true;
                        QnMessageBox::critical(this,
                            tr("Launcher process was not found."),
                            tr("Cannot restart the client.") + L'\n'
                            + tr("Please close the application and start it again using the shortcut in the start menu."));
                    }
                    else
                    {
                        qApp->exit(0);
                        applauncher::scheduleProcessKill(QCoreApplication::applicationPid(), kProcessTerminateTimeoutMs);
                    }
                }

                if (unholdConnection)
                    qnClientMessageProcessor->setHoldConnection(false);

                if (result.protocolChanged)
                    menu()->trigger(QnActions::DisconnectAction, QnActionParameters().withArgument(Qn::ForceRole, true));

                break;
            }

            case QnUpdateResult::Cancelled:
                QnMessageBox::information(this, tr("Update cancelled"), result.errorMessage());
                break;

            case QnUpdateResult::AlreadyUpdated:
                QnMessageBox::information(this, tr("Update is not needed."), result.errorMessage());
                break;

            case QnUpdateResult::LockFailed:
            case QnUpdateResult::DownloadingFailed:
            case QnUpdateResult::DownloadingFailed_NoFreeSpace:
            case QnUpdateResult::UploadingFailed:
            case QnUpdateResult::UploadingFailed_NoFreeSpace:
            case QnUpdateResult::UploadingFailed_Timeout:
            case QnUpdateResult::UploadingFailed_Offline:
            case QnUpdateResult::UploadingFailed_AuthenticationFailed:
            case QnUpdateResult::ClientInstallationFailed:
            case QnUpdateResult::InstallationFailed:
            case QnUpdateResult::RestInstallationFailed:
                QnMessageBox::critical(this, tr("Update unsuccessful."), result.errorMessage());
                break;
        }
    }

    bool canUpdate = (result.result != QnUpdateResult::Successful) &&
        (result.result != QnUpdateResult::AlreadyUpdated);

    ui->updateButton->setEnabled(canUpdate);
}
