#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

#include <ui/common/palette.h>
#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <update/media_server_update_tool.h>

#include <utils/applauncher_utils.h>
#include <utils/common/app_info.h>


namespace {
    const int longInstallWarningTimeout = 2 * 60 * 1000; // 2 minutes
    // Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
    const quint32 processTerminateTimeout = 15000;

    const int tooLateDayOfWeek = Qt::Thursday;

    const int autoCheckIntervalMs = 5 * 60 * 1000;  // 5 minutes
}

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnServerUpdatesWidget),
    m_latestVersion(qnCommon->engineVersion()),
    m_checkingInternet(false),
    m_checkingLocal(false)
{
    ui->setupUi(this);
    setWarningStyle(ui->dayWarningLabel);

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

    connect(ui->cancelButton,           &QPushButton::clicked,      m_updateTool, &QnMediaServerUpdateTool::cancelUpdate);
    connect(ui->internetUpdateButton,   &QPushButton::clicked,      this, [this] {
        m_updateTool->startUpdate(m_targetVersion, !m_targetVersion.isNull());
    });
    ui->internetUpdateButton->setEnabled(false);
    ui->internetDetailLabel->setVisible(false);

    connect(ui->localUpdateButton,      &QPushButton::clicked,      this, [this] {
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

    initSourceMenu();
    initLinkButtons();
    initBuildSelectionButtons();

    connect(m_updateTool,       &QnMediaServerUpdateTool::stageChanged,             this,           &QnServerUpdatesWidget::at_tool_stageChanged);
    connect(m_updateTool,       &QnMediaServerUpdateTool::stageProgressChanged,     this,           &QnServerUpdatesWidget::at_tool_stageProgressChanged);
    connect(m_updateTool,       &QnMediaServerUpdateTool::updateFinished,           this,           &QnServerUpdatesWidget::at_updateFinished);

    setWarningStyle(ui->dayWarningLabel);
    setWarningStyle(ui->connectionProblemLabel);

    static_assert(tooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningLabel->setVisible(false);
    ui->connectionProblemLabel->setVisible(false);

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

        checkForUpdatesInternet();
    });
    updateTimer->start();

    at_tool_stageChanged(QnFullUpdateStage::Init);
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

    connect(ui->saveLinkButton, &QPushButton::clicked, this, [this] {
        QString fileName = QnFileDialog::getSaveFileName(this, tr("Save to file..."), QString(), tr("Url Files (*.url)"), 0, QnCustomFileDialog::fileDialogOptions());
        if (fileName.isEmpty())
            return;
        QString selectedExtension = lit(".url");
        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;
            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    this,
                    tr("Save to file..."),
                    tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).completeBaseName()),
                    QMessageBox::Ok | QMessageBox::Cancel
                    );
                if(button == QMessageBox::Cancel)
                    return;
            }
        }

        QFile data(fileName);
        if (!data.open(QFile::WriteOnly | QFile::Truncate)) {
            QMessageBox::critical(this, tr("Error"), tr("Could not save url to file %1").arg(fileName));
            return;
        }

        QString package = ui->linkLineEdit->text();
        QTextStream out(&data);
        out << lit("[InternetShortcut]") << endl << package << endl;

        data.close();
    });
    
    connect(ui->linkLineEdit,           &QLineEdit::textChanged,    this, [this](const QString &text){
        ui->copyLinkButton->setEnabled(!text.isEmpty());
        ui->saveLinkButton->setEnabled(!text.isEmpty());
        ui->linkLineEdit->setCursorPosition(0);
    });

    ui->copyLinkButton->setEnabled(false);
    ui->saveLinkButton->setEnabled(false);

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
        ui->versionTitleLabel->setText(tr("Latest version:"));

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
        ui->versionTitleLabel->setText(tr("Target version:"));

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

void QnServerUpdatesWidget::updateFromSettings() {
    if (!m_updateTool->idle())
        return;

    checkForUpdatesInternet(true);
}

bool QnServerUpdatesWidget::confirm() {
    if (isUpdating()) {
        QMessageBox::information(this, tr("Information"), tr("Update is in process now."));
        return false;
    }

    return true;
}

bool QnServerUpdatesWidget::discard() {
    if(!cancelUpdate()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot cancel update at this state."));
        return false;
    }

    return true;
}

void QnServerUpdatesWidget::at_updateFinished(const QnUpdateResult &result) {
    ui->updateProgessBar->setValue(100);
    ui->updateProgessBar->setFormat(tr("Update finished... 100%"));

    switch (result.result) {
    case QnUpdateResult::Successful:
        {
            QString message = tr("Update has been successfully finished.");

            message += lit("\n");
            if (result.clientInstallerRequired)
                message += tr("Now you have to update the client using an installer.");
            else
                message += tr("The client will be restarted to the updated version.");


            QMessageBox::information(this, tr("Update is successful"), message);

            if (!result.clientInstallerRequired) {
                if (!applauncher::restartClient(result.targetVersion) == applauncher::api::ResultType::ok) {
                    QMessageBox::critical(this,
                        tr("Launcher process is not found"),
                        tr("Cannot restart the client.\n"
                        "Please close the application and start it again using the shortcut in the start menu."));
                } else {
                    qApp->exit(0);
                    applauncher::scheduleProcessKill(QCoreApplication::applicationPid(), processTerminateTimeout);
                }
            }
        }
        break;
    case QnUpdateResult::Cancelled:
        QMessageBox::information(this, tr("Update cancelled"), tr("Update has been cancelled."));
        break;
    case QnUpdateResult::LockFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Someone has already started an update."));
        break;
    case QnUpdateResult::DownloadingFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not download updates."));
        break;
    case QnUpdateResult::DownloadingFailed_NoFreeSpace:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not download updates.") + lit("\n") + tr("No free space left on the disk."));
        break;
    case QnUpdateResult::UploadingFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not push updates to servers."));
        break;
    case QnUpdateResult::UploadingFailed_NoFreeSpace:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not push updates to servers.") + lit("\n") + tr("No free space left on the server."));
        break;
    case QnUpdateResult::ClientInstallationFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not install an update to the client."));
        break;
    case QnUpdateResult::InstallationFailed:
    case QnUpdateResult::RestInstallationFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not install updates on one or more servers."));
        break;
    }

    bool canUpdate = result.result != QnUpdateResult::Successful;
    if (m_updateSourceActions[LocalSource]->isChecked())
        ui->localUpdateButton->setEnabled(canUpdate);
    else
        ui->internetUpdateButton->setEnabled(canUpdate);
}

void QnServerUpdatesWidget::checkForUpdatesInternet(bool autoSwitch, bool autoStart) {
    if (m_checkingInternet || !m_updateTool->idle())
        return;
    m_checkingInternet = true;

    ui->internetUpdateButton->setEnabled(false);
    ui->latestBuildLabel->setEnabled(false);
    ui->specificBuildLabel->setEnabled(false);
    ui->internetRefreshWidget->setCurrentWidget(ui->internetProgressPage);
    ui->internetDetailLabel->setPalette(this->palette());
    ui->internetDetailLabel->setText(tr("Checking for updates..."));
    ui->internetDetailLabel->setVisible(true);
    ui->latestVersionLabel->setPalette(this->palette());

    QnSoftwareVersion targetVersion = m_targetVersion;

    m_updateTool->checkForUpdates(m_targetVersion, !m_targetVersion.isNull(), [this, autoSwitch, autoStart, targetVersion](const QnCheckForUpdateResult &result) {
        QPalette detailPalette = this->palette();
        QPalette statusPalette = this->palette();
        QString detail;
        QString status;

        /* Update latest version if we have checked for updates in the internet. */
        if (targetVersion.isNull() && !result.latestVersion.isNull()) {
            m_latestVersion = result.latestVersion;
            m_updatesModel->setLatestVersion(m_latestVersion);
        }
        QnSoftwareVersion displayVersion = targetVersion.isNull()
            ? m_latestVersion
            : targetVersion;

        switch (result.result) {
        case QnCheckForUpdateResult::UpdateFound:
        case QnCheckForUpdateResult::NoNewerVersion:
            status = displayVersion.toString();
            break;
        case QnCheckForUpdateResult::InternetProblem:
            status = tr("Internet connection problem");
            setWarningStyle(&statusPalette);
            break;
        case QnCheckForUpdateResult::NoSuchBuild:
            status = displayVersion.toString();
            detail = tr("There is no such build on the update server");
            setWarningStyle(&statusPalette);
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ServerUpdateImpossible:
            status = displayVersion.toString();
            detail = tr("Cannot start update. An update for one or more servers was not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ClientUpdateImpossible:
            status = displayVersion.toString();
            detail = tr("Cannot start update. An update for the client was not found.");
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
            m_updateTool->startUpdate(targetVersion, !targetVersion.isNull());
    });

}

void QnServerUpdatesWidget::checkForUpdatesLocal() {
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
            if (!result.latestVersion.isNull() && result.clientInstallerRequired)
                detail = tr("Newer version found. You will have to update the client manually using an installer.");
            break;
        case QnCheckForUpdateResult::NoNewerVersion:
            detail = tr("All components in your system are up to date.");
            break;
        case QnCheckForUpdateResult::NoSuchBuild:
            detail = tr("There is no such build on the update server");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ServerUpdateImpossible:
            detail = tr("Cannot start update. An update for one or more servers was not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ClientUpdateImpossible:
            detail = tr("Cannot start update. An update for the client was not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::BadUpdateFile:
            detail = tr("Cannot update from this file.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::NoFreeSpace:
            detail = tr("Cannot extract the update file.") + lit("\n") + tr("No free space left on the disk.");
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

