#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <client/client_message_processor.h>

#include <ui/common/palette.h>
#include <ui/common/ui_resource_name.h>
#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>
#include <ui/actions/action_manager.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench_access_controller.h>

#include <update/media_server_update_tool.h>

#include <utils/applauncher_utils.h>
#include <utils/common/app_info.h>


namespace {
    const int longInstallWarningTimeout = 2 * 60 * 1000; // 2 minutes
    // Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
    const quint32 processTerminateTimeout = 15000;

    const int tooLateDayOfWeek = Qt::Thursday;

    const int autoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour

    const int maxLabelWidth = 400; // pixels
}

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnServerUpdatesWidget),
    m_latestVersion(qnCommon->engineVersion()),
    m_checkingInternet(false),
    m_checkingLocal(false),
    m_longUpdateWarningTimer(new QTimer(this)),
    m_lastAutoUpdateCheck(0)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::Administration_Update_Help);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(m_updateTool, this);

    QnSortedServerUpdatesModel *sortedUpdatesModel = new QnSortedServerUpdatesModel(this);
    sortedUpdatesModel->setSourceModel(m_updatesModel);
    sortedUpdatesModel->sort(0); // the column does not matter because the model uses column-independent sorting

    ui->tableView->setModel(sortedUpdatesModel);
    ui->tableView->setItemDelegateForColumn(QnServerUpdatesModel::VersionColumn, new QnUpdateStatusItemDelegate(ui->tableView));

    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::NameColumn, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::VersionColumn, QHeaderView::ResizeToContents);   
    ui->tableView->horizontalHeader()->setSectionsClickable(false);
    setPaletteColor(ui->tableView, QPalette::Highlight, Qt::transparent);

    connect(ui->cancelButton,           &QPushButton::clicked,      this, [this] {
        if (!accessController()->hasGlobalPermissions(Qn::GlobalProtectedPermission))
            return;
        m_updateTool->cancelUpdate();
    });

    connect(ui->internetUpdateButton,   &QPushButton::clicked,      this, [this] {
        if (!accessController()->hasGlobalPermissions(Qn::GlobalProtectedPermission))
            return;
        m_updateTool->startUpdate(m_targetVersion);
    });
    ui->internetUpdateButton->setEnabled(false);
    ui->internetDetailLabel->setVisible(false);

    connect(ui->localUpdateButton,      &QPushButton::clicked,      this, [this] {
        if (!accessController()->hasGlobalPermissions(Qn::GlobalProtectedPermission))
            return;
        m_updateTool->startUpdate(ui->filenameLineEdit->text());
    });
    ui->localUpdateButton->setEnabled(false);
    ui->localDetailLabel->setVisible(false);

    connect(ui->refreshButton,          &QPushButton::clicked,      this, [this] {
        checkForUpdatesInternet();
    });
    ui->refreshButton->setIcon(qnSkin->icon("refresh.png"));

    connect(ui->browseFileButton,       &QPushButton::clicked,      this, [this] {
        QString fileName = QnFileDialog::getOpenFileName(this, 
            tr("Select Update File..."),
            QString(), tr("Update Files (*.zip)"), 
            0,
            QnCustomFileDialog::fileDialogOptions());
        if (fileName.isEmpty()) 
            return;
        ui->filenameLineEdit->setText(fileName);
        checkForUpdatesLocal();
    });

    connect(ui->updatesNotificationCheckbox,    &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);

    initSourceMenu();
    initLinkButtons();
    initBuildSelectionButtons();

    connect(m_updateTool,       &QnMediaServerUpdateTool::stageChanged,             this,           &QnServerUpdatesWidget::at_tool_stageChanged);
    connect(m_updateTool,       &QnMediaServerUpdateTool::stageProgressChanged,     this,           &QnServerUpdatesWidget::at_tool_stageProgressChanged);
    connect(m_updateTool,       &QnMediaServerUpdateTool::updateFinished,           this,           &QnServerUpdatesWidget::at_updateFinished);

    setWarningStyle(ui->dayWarningLabel);
    ui->dayWarningLabel->setText(tr("Caution: Applying system updates at the end of the week is not recommended."));

    setWarningStyle(ui->connectionProblemLabel);
    setWarningStyle(ui->longUpdateWarning);

    static_assert(tooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningLabel->setVisible(false);
    ui->connectionProblemLabel->setVisible(false);
    ui->longUpdateWarning->setVisible(false);

    ui->releaseNotesLabel->setText(lit("<a href='notes'>%1</a>").arg(tr("Release notes")));
    ui->specificBuildLabel->setText(lit("<a href='spec'>%1</a>").arg(tr("Get a specific build")));
    ui->latestBuildLabel->setText(lit("<a href='latest'>%1</a>").arg(tr("Get the latest version")));

    QTimer* updateTimer = new QTimer(this);
    updateTimer->setSingleShot(false);
    updateTimer->setInterval(autoCheckIntervalMs);
    connect(updateTimer, &QTimer::timeout, this, [this] {
        /* Do not check while updating. */
        if (!m_updateTool->idle())
            return;

        /* Do not recheck specific build. */
        if (!m_targetVersion.isNull())
            return;

        autoCheckForUpdatesInternet();
    });
    updateTimer->start();

    m_longUpdateWarningTimer->setInterval(longInstallWarningTimeout);
    m_longUpdateWarningTimer->setSingleShot(true);
    connect(m_longUpdateWarningTimer, &QTimer::timeout, ui->longUpdateWarning, &QLabel::show);

    connect(qnResPool, &QnResourcePool::statusChanged, this, [this] (const QnResourcePtr &resource) {
        if (!resource->hasFlags(Qn::server))
            return;

        ui->linkLineEdit->setText(m_updateTool->generateUpdatePackageUrl(m_targetVersion).toString());
    });

    at_tool_stageChanged(QnFullUpdateStage::Init);
    checkForUpdatesInternet(true);
}


void QnServerUpdatesWidget::initSourceMenu() {
    QMenu *menu = new QMenu(this);
    QActionGroup* actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    m_updateSourceActions[InternetSource] = menu->addAction(tr("Update from Internet..."));
    m_updateSourceActions[LocalSource] = menu->addAction(tr("Update from local source..."));

    for (QAction* action: m_updateSourceActions) {
        action->setCheckable(true);
        action->setActionGroup(actionGroup);
    }
    m_updateSourceActions[InternetSource]->setChecked(true);

    {   /* Setup 'Source' button */
        ui->sourceButton->setText(tr("Update from Internet"));
        QPalette palette(this->palette());
        palette.setColor(QPalette::Link, palette.color(QPalette::WindowText));
        ui->sourceButton->setPalette(palette);

        ui->sourceButton->setIcon(qnSkin->icon("buttons/dropdown_menu.png"));
        connect(ui->sourceButton, &QPushButton::clicked, this, [this, menu] {
            if (menu->isVisible())
                return;
            QPoint local = ui->sourceButton->geometry().bottomLeft();
            menu->popup(ui->sourceButton->parentWidget()->mapToGlobal(local));
        });
    }

    connect(m_updateSourceActions[InternetSource], &QAction::triggered, this, [this] {
        if (!m_updateTool->idle())
            return;
        
        ui->sourceWidget->setCurrentWidget(ui->internetPage);
        ui->sourceButton->setText(tr("Update from Internet"));
        checkForUpdatesInternet();
    });

    connect(m_updateSourceActions[LocalSource], &QAction::triggered, this, [this] {
        if (!m_updateTool->idle())
            return;

        ui->sourceWidget->setCurrentWidget(ui->localPage);
        ui->sourceButton->setText(tr("Update from local source"));
    });
}


void QnServerUpdatesWidget::initLinkButtons() {
    connect(ui->copyLinkButton, &QPushButton::clicked, this, [this] {
        qApp->clipboard()->setText(ui->linkLineEdit->text());
        QMessageBox::information(this, tr("Success"), tr("URL copied to clipboard."));
    });
 
    connect(ui->linkLineEdit,           &QLineEdit::textChanged,    this, [this](const QString &text){
        ui->copyLinkButton->setEnabled(!text.isEmpty());
        ui->linkLineEdit->setCursorPosition(0);
    });

    ui->copyLinkButton->setEnabled(false);

    connect(ui->releaseNotesLabel,  &QLabel::linkActivated, this, [this] {
        if (!m_releaseNotesUrl.isEmpty())
            QDesktopServices::openUrl(m_releaseNotesUrl);
    });
}

void QnServerUpdatesWidget::initBuildSelectionButtons() {
    connect(ui->latestBuildLabel,   &QLabel::linkActivated, this, [this] {
        m_targetVersion = QnSoftwareVersion();
        ui->linkLineEdit->setText(m_updateTool->generateUpdatePackageUrl(m_targetVersion).toString());
        ui->latestBuildLabel->setVisible(false);
        ui->specificBuildLabel->setVisible(true);

        ui->latestVersionLabel->setText(m_latestVersion.toString());
        ui->versionTitleLabel->setText(tr("Latest Version:"));

        checkForUpdatesInternet(true);
    });

    connect(ui->specificBuildLabel,   &QLabel::linkActivated, this, [this] {
        QnBuildNumberDialog dialog(this);
        if (!dialog.exec())
            return;

        QnSoftwareVersion version = qnCommon->engineVersion();
        m_targetVersion = QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber());

        ui->linkLineEdit->setText(m_updateTool->generateUpdatePackageUrl(m_targetVersion).toString());
        ui->specificBuildLabel->setVisible(false);
        ui->latestBuildLabel->setVisible(true);

        ui->latestVersionLabel->setText(m_targetVersion.toString());
        ui->versionTitleLabel->setText(tr("Target Version:"));

        checkForUpdatesInternet(true, true);
    });

    ui->linkLineEdit->setText(m_updateTool->generateUpdatePackageUrl(m_targetVersion).toString());
    ui->latestBuildLabel->setVisible(false);
}

bool QnServerUpdatesWidget::cancelUpdate() {
    if (m_updateTool->isUpdating())
        return m_updateTool->cancelUpdate();
    return true;
}

bool QnServerUpdatesWidget::isUpdating() const {
    return m_updateTool->isUpdating();
}

QnMediaServerUpdateTool *QnServerUpdatesWidget::updateTool() const {
    return m_updateTool;
}

void QnServerUpdatesWidget::loadDataToUi() {
    ui->updatesNotificationCheckbox->setChecked(QnGlobalSettings::instance()->isUpdateNotificationsEnabled());
}

void QnServerUpdatesWidget::applyChanges() {
    QnGlobalSettings::instance()->setUpdateNotificationsEnabled(ui->updatesNotificationCheckbox->isChecked());
}

bool QnServerUpdatesWidget::hasChanges() const {
    if (isReadOnly())
        return false;

    return QnGlobalSettings::instance()->isUpdateNotificationsEnabled() != ui->updatesNotificationCheckbox->isChecked();
}

bool QnServerUpdatesWidget::canApplyChanges() {
    //TODO: #GDM now this prevents other tabs from saving their changes
    if (isUpdating()) {
        QMessageBox::information(this, tr("Information"), tr("Update is in process now."));
        return false;
    }
   
    return true;
}

bool QnServerUpdatesWidget::canDiscardChanges() {
    //TODO: #GDM now this prevents other tabs from discarding their changes
    if(!cancelUpdate()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot cancel update at this state.") + L'\n' + tr("Please wait until update is finished"));
        return false;
    }

    return true;
}

void QnServerUpdatesWidget::at_updateFinished(const QnUpdateResult &result) {
    ui->updateProgessBar->setValue(100);
    ui->updateProgessBar->setFormat(tr("Update Finished...100%"));

    if (isVisible()) {
        switch (result.result) {
        case QnUpdateResult::Successful:
            {
                QString message = tr("Update has been successfully finished.");

                bool clientUpdated = (result.targetVersion != qnCommon->engineVersion());

                if (clientUpdated) {
                    message += lit("\n");
                    if (result.clientInstallerRequired) {
#ifdef Q_OS_MAC
                        message += tr("Please update the client manually.");
#else
                        message += tr("Please update the client manually using an installation package.");
#endif
                    } else {
                        message += tr("The client will be restarted to the updated version.");
                    }
                }

                QMessageBox::information(this, tr("Update Succeeded."), message);

                bool unholdConnection = !clientUpdated || result.clientInstallerRequired || result.protocolChanged;
                if (clientUpdated && !result.clientInstallerRequired) {
                    if (applauncher::restartClient(result.targetVersion) != applauncher::api::ResultType::ok) {
                        unholdConnection = true;
                        QMessageBox::critical(this,
                            tr("Launcher process not found."),
                            tr("Cannot restart the client.") + L'\n' 
                          + tr("Please close the application and start it again using the shortcut in the start menu."));
                    } else {
                        qApp->exit(0);
                        applauncher::scheduleProcessKill(QCoreApplication::applicationPid(), processTerminateTimeout);
                    }
                }

                if (unholdConnection)
                    qnClientMessageProcessor->setHoldConnection(false);

                if (result.protocolChanged)
                    menu()->trigger(Qn::DisconnectAction, QnActionParameters().withArgument(Qn::ForceRole, true));
            }
            break;
        case QnUpdateResult::Cancelled:
            QMessageBox::information(this, tr("Update Cancelled"), tr("Update has been cancelled."));
            break;
        case QnUpdateResult::LockFailed:
            QMessageBox::critical(this, tr("Update unsuccessful."), tr("Another user has already started an update."));
            break;
        case QnUpdateResult::DownloadingFailed:
            QMessageBox::critical(this, tr("Update unsuccessful."), tr("Could not download updates."));
            break;
        case QnUpdateResult::DownloadingFailed_NoFreeSpace:
            QMessageBox::critical(this, tr("Update unsuccessful."), tr("Could not download updates.") + lit("\n") + tr("No free space left on the disk."));
            break;
        case QnUpdateResult::UploadingFailed: {
            QString message = tr("Could not push updates to servers.");
            if (!result.failedPeers.isEmpty()) {
                message += lit("\n");
                message += tr("The problem is caused by %n servers:", "", result.failedPeers.size());
                message += lit("\n");
                message += serverNamesString(result.failedPeers);
            }
            QMessageBox::critical(this, tr("Update unsuccessful."), message);
            break;
        }
        case QnUpdateResult::UploadingFailed_NoFreeSpace:
            QMessageBox::critical(this, tr("Update unsuccessful."),
                                  tr("Could not push updates to servers.") +
                                  lit("\n") +
                                  tr("No free space left on %n servers:", "", result.failedPeers.size()) +
                                  lit("\n") +
                                  serverNamesString(result.failedPeers));
            break;
        case QnUpdateResult::UploadingFailed_Timeout:
            QMessageBox::critical(this, tr("Update unsuccessful."),
                                  tr("Could not push updates to servers.") +
                                  lit("\n") +
                                  tr("%n servers are not responding:", "", result.failedPeers.size()) +
                                  lit("\n") +
                                  serverNamesString(result.failedPeers));
            break;
        case QnUpdateResult::UploadingFailed_Offline:
            QMessageBox::critical(this, tr("Update unsuccessful."),
                                  tr("Could not push updates to servers.") +
                                  lit("\n") +
                                  tr("%n servers have gone offline:", "", result.failedPeers.size()) +
                                  lit("\n") +
                                  serverNamesString(result.failedPeers));
            break;
        case QnUpdateResult::ClientInstallationFailed:
            QMessageBox::critical(this, tr("Update unsuccessful."), tr("Could not install an update to the client."));
            break;
        case QnUpdateResult::InstallationFailed:
        case QnUpdateResult::RestInstallationFailed:
            QMessageBox::critical(this, tr("Update unsuccessful."), tr("Could not install updates on one or more servers."));
            break;
        }
    }

    bool canUpdate = result.result != QnUpdateResult::Successful;
    if (m_updateSourceActions[LocalSource]->isChecked())
        ui->localUpdateButton->setEnabled(canUpdate);
    else
        ui->internetUpdateButton->setEnabled(canUpdate);
}

void QnServerUpdatesWidget::autoCheckForUpdatesInternet() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAutoUpdateCheck < autoCheckIntervalMs)
        return;
    checkForUpdatesInternet();
}

void QnServerUpdatesWidget::checkForUpdatesInternet(bool autoSwitch, bool autoStart) {
    if (!accessController()->hasGlobalPermissions(Qn::GlobalProtectedPermission))
        return;

    if (m_checkingInternet || !m_updateTool->idle())
        return;
    m_checkingInternet = true;
    m_lastAutoUpdateCheck = QDateTime::currentMSecsSinceEpoch();

    ui->internetUpdateButton->setEnabled(false);
    ui->latestBuildLabel->setEnabled(false);
    ui->specificBuildLabel->setEnabled(false);
    ui->internetRefreshWidget->setCurrentWidget(ui->internetProgressPage);
    ui->internetDetailLabel->setPalette(this->palette());
    ui->internetDetailLabel->setText(tr("Checking for updates..."));
    ui->internetDetailLabel->setVisible(true);
    ui->latestVersionLabel->setPalette(this->palette());

    QnSoftwareVersion targetVersion = m_targetVersion;

    m_updateTool->checkForUpdates(m_targetVersion, [this, autoSwitch, autoStart, targetVersion](const QnCheckForUpdateResult &result) {
        QPalette detailPalette = this->palette();
        QPalette statusPalette = this->palette();
        QString detail;
        QString status;

        /* Update latest version if we have checked for updates in the internet. */
        if (targetVersion.isNull() && !result.version.isNull()) {
            m_latestVersion = result.version;
            m_updatesModel->setLatestVersion(m_latestVersion);
        }
        QnSoftwareVersion displayVersion = result.version.isNull()
            ? m_latestVersion
            : result.version;

        switch (result.result) {
        case QnCheckForUpdateResult::UpdateFound:
        case QnCheckForUpdateResult::NoNewerVersion:
            status = displayVersion.toString();
            break;
        case QnCheckForUpdateResult::InternetProblem:
            status = tr("Internet Connectivity Problem");
            setWarningStyle(&statusPalette);
            break;
        case QnCheckForUpdateResult::NoSuchBuild:
            status = displayVersion.toString();
            detail = tr("No such build available on update server.");
            setWarningStyle(&statusPalette);
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ServerUpdateImpossible:
            status = displayVersion.toString();
            detail = tr("Unable to begin update. An update for one or more servers not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ClientUpdateImpossible:
            status = displayVersion.toString();
            detail = tr("Unable to begin update. An update for the client was not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::DowngradeIsProhibited:
            status = displayVersion.toString();
            detail = tr("Unable to begin update. Downgrade to the previous release is prohibited.");
            setWarningStyle(&statusPalette);
            setWarningStyle(&detailPalette);
            break;
        default:
            Q_ASSERT(false);    //should never get here
        }

        if (autoSwitch && result.result == QnCheckForUpdateResult::InternetProblem)
            m_updateSourceActions[LocalSource]->trigger();
        
        m_releaseNotesUrl = result.releaseNotesUrl;

        ui->connectionProblemLabel->setVisible(result.result == QnCheckForUpdateResult::InternetProblem);

        ui->internetDetailLabel->setText(detail);
        ui->internetDetailLabel->setPalette(detailPalette);
        ui->internetDetailLabel->setVisible(!detail.isEmpty());
        ui->latestVersionLabel->setText(status);
        ui->latestVersionLabel->setPalette(statusPalette);
        ui->releaseNotesLabel->setVisible(!m_releaseNotesUrl.isEmpty());
        ui->latestBuildLabel->setEnabled(true);
        ui->specificBuildLabel->setEnabled(true);

        m_checkingInternet = false;
        ui->internetUpdateButton->setEnabled(result.result == QnCheckForUpdateResult::UpdateFound && m_updateTool->idle());
        ui->internetRefreshWidget->setCurrentWidget(ui->internetRefreshPage);

        m_updatesModel->setCheckResult(result);

        if (autoStart && result.result == QnCheckForUpdateResult::UpdateFound)
            m_updateTool->startUpdate(result.version);
    });

}

void QnServerUpdatesWidget::checkForUpdatesLocal() {
    if (!accessController()->hasGlobalPermissions(Qn::GlobalProtectedPermission))
        return;

    if (m_checkingLocal)
        return;
    m_checkingLocal = true;

    ui->localUpdateButton->setEnabled(false);
    ui->localRefreshWidget->setCurrentWidget(ui->localProgressPage);
    ui->localDetailLabel->setPalette(this->palette());
    ui->localDetailLabel->setText(tr("Checking for updates..."));
    ui->localDetailLabel->setVisible(true);
    ui->latestVersionLabel->setPalette(this->palette());

    m_updateTool->checkForUpdates(ui->filenameLineEdit->text(), [this](const QnCheckForUpdateResult &result) {
        QPalette detailPalette = this->palette();
        QString detail;

        switch (result.result) {
        case QnCheckForUpdateResult::UpdateFound:
            if (!result.version.isNull() && result.clientInstallerRequired) {
                detail = tr("Newer version found.");
                detail += lit("\n");
#ifdef Q_OS_MAC
                detail += tr("You will have to update the client manually.");
#else
                detail += tr("You will have to update the client manually using an installer.");
#endif
            }
            break;
        case QnCheckForUpdateResult::NoNewerVersion:
            detail = tr("All components in your system are up to date.");
            break;
        case QnCheckForUpdateResult::NoSuchBuild:
            detail = tr("No such build available on update server.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ServerUpdateImpossible:
            detail = tr("Unable to begin update. An update for one or more servers not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ClientUpdateImpossible:
            detail = tr("Unable to begin update. An update for the client was not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::BadUpdateFile:
            detail = tr("Cannot update from this file.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::NoFreeSpace:
            detail = tr("Unable to extract update file.") + lit("\n") + tr("No free space left on the disk.");
            setWarningStyle(&detailPalette);
            break;
        default:
            Q_ASSERT(false);    //should never get here
        }

        ui->localDetailLabel->setText(detail);
        ui->localDetailLabel->setPalette(detailPalette);
        ui->localDetailLabel->setVisible(!detail.isEmpty());
        ui->releaseNotesLabel->hide();

        m_checkingLocal = false;
        ui->localUpdateButton->setEnabled(result.result == QnCheckForUpdateResult::UpdateFound && m_updateTool->idle());
        ui->localRefreshWidget->setCurrentWidget(ui->localBrowsePage);

        m_updatesModel->setCheckResult(result);
    });
}

void QnServerUpdatesWidget::at_tool_stageChanged(QnFullUpdateStage stage) {
    ui->sourceButton->setEnabled(m_updateTool->idle());
    for (QAction* action: m_updateSourceActions)
        action->setEnabled(m_updateTool->idle());

    if (stage != QnFullUpdateStage::Init) {
        ui->internetUpdateButton->setEnabled(false);
        ui->localUpdateButton->setEnabled(false);
    } else { /* Stage returned to idle, update finished. */
        ui->longUpdateWarning->hide();
        m_longUpdateWarningTimer->stop();
    }

    ui->dayWarningLabel->setVisible(QDateTime::currentDateTime().date().dayOfWeek() >= tooLateDayOfWeek);

    ui->progressStatusWidget->setCurrentWidget(stage == QnFullUpdateStage::Init
        ? ui->updateTargetPage
        : ui->updateRunningPage
        );

    bool cancellable = false;

    switch (stage) {
    case QnFullUpdateStage::Check:
    case QnFullUpdateStage::Download:
    case QnFullUpdateStage::Client:
    case QnFullUpdateStage::Incompatible:
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

void QnServerUpdatesWidget::at_tool_stageProgressChanged(QnFullUpdateStage stage, int progress) {

    int offset = 0;
    switch (stage) {
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
    int value = (displayStage*100 + offset + progress) / ( static_cast<int>(QnFullUpdateStage::Count) - 1 );

    QString status;
    switch (stage) {
    case QnFullUpdateStage::Check:
        status = tr("Checking for updates... %1%");
        break;
    case QnFullUpdateStage::Download:
        status = tr("Downloading updates... %1%");
        break;
    case QnFullUpdateStage::Client:
        status = tr("Installing client update... %1%");
        break;
    case QnFullUpdateStage::Incompatible:
        status = tr("Installing updates to incompatible servers... %1%");
        break;
    case QnFullUpdateStage::Push:
        status = tr("Pushing updates to servers... %1%");
        break;
    case QnFullUpdateStage::Servers:
        status = tr("Installing updates... %1%");
        break;
    default:
        status = lit("%1");
        break;
    }
    ui->updateProgessBar->setValue(value);
    ui->updateProgessBar->setFormat(status.arg(value));
}

QString QnServerUpdatesWidget::serverNamesString(const QSet<QnUuid> &serverIds) {
    QString result;

    for (const QnUuid &id: serverIds) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(id);
        if (!server)
            continue;

        if (!result.isEmpty())
            result += lit("\n");

        QString name = getResourceName(server);
        result.append(fontMetrics().elidedText(name, Qt::ElideMiddle, maxLabelWidth));
    }

    return result;
}
