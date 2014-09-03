#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtWidgets/QMessageBox>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <utils/media_server_update_tool.h>
#include <utils/applauncher_utils.h>

#include <version.h>

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
    m_extraMessageTimer(new QTimer(this))
{
    ui->setupUi(this);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(this);

    QnSortedServerUpdatesModel *sortedUpdatesModel = new QnSortedServerUpdatesModel(this);
    sortedUpdatesModel->setSourceModel(m_updatesModel);
    sortedUpdatesModel->sort(0); // the column does not matter because the model uses column-independent sorting

    ui->tableView->setModel(sortedUpdatesModel);
    ui->tableView->setItemDelegateForColumn(QnServerUpdatesModel::VersionColumn, new QnUpdateStatusItemDelegate(ui->tableView));

    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::NameColumn, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::VersionColumn, QHeaderView::ResizeToContents);   

    connect(ui->cancelButton,       &QPushButton::clicked,      m_updateTool,   &QnMediaServerUpdateTool::cancelUpdate);
    connect(ui->startUpdateButton,  &QPushButton::clicked,      this, [this] {
        if (!m_updateTool->idle())
            return;
        m_updateTool->updateServers();
    });

    connect(m_updateTool,       &QnMediaServerUpdateTool::stateChanged,         this,           &QnServerUpdatesWidget::updateUi);
    connect(m_updateTool,       &QnMediaServerUpdateTool::progressChanged,      ui->updateProgessBar,   &QProgressBar::setValue);
    connect(m_updateTool,       &QnMediaServerUpdateTool::peerChanged,          this,           [this](const QUuid &peerId) {
        m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(peerId));
    });
    connect(m_updateTool,       &QnMediaServerUpdateTool::checkForUpdatesFinished,  this, &QnServerUpdatesWidget::at_checkForUpdatesFinished);
    connect(m_updateTool,       &QnMediaServerUpdateTool::updateFinished,           this, &QnServerUpdatesWidget::at_updateFinished);

    m_extraMessageTimer->setInterval(longInstallWarningTimeout);
    connect(m_extraMessageTimer, &QTimer::timeout, this, [this] {
        if (m_updateTool->state() == QnMediaServerUpdateTool::InstallingUpdate)
            ui->extraMessageLabel->show();
    });

    setWarningStyle(ui->dayWarningLabel);
    static_assert(tooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningLabel->setVisible(false);
    ui->detailWidget->setVisible(false);   

    QList<int> progressSeparators;
    for(int i = 1; i < static_cast<int>(QnFullUpdateStage::Count); ++i) {
        progressSeparators.append(i * 100 / static_cast<int>(QnFullUpdateStage::Count));
    }
    ui->updateProgessBar->setSeparators(progressSeparators);

    initMenu();

    m_updateInfo.source = InternetSource;

    QTimer* updateTimer = new QTimer(this);
    updateTimer->setSingleShot(false);
    updateTimer->setInterval(autoCheckIntervalMs);
    connect(updateTimer, &QTimer::timeout, this, [this] {
        /* Auto refresh only if updates are loaded from internet. */
        if (m_updateInfo.source != InternetSource)
            return;

        /* Do not check while updating. */
        if (!m_updateTool->idle())
            return;

        checkForUpdates();
    });
    updateTimer->start();

    updateUi();
    checkForUpdates();
}

void QnServerUpdatesWidget::initMenu() {
    QMenu *menu = new QMenu(this);
    QActionGroup* actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    m_updateSourceActions[InternetSource] = menu->addAction(tr("Internet..."));
    m_updateSourceActions[LocalSource] = menu->addAction(tr("Local source..."));
    m_updateSourceActions[SpecificBuildSource] = menu->addAction(tr("Specific build..."));

    for (QAction* action: m_updateSourceActions) {
        action->setCheckable(true);
        action->setActionGroup(actionGroup);
    }

    connect(m_updateSourceActions[InternetSource], &QAction::triggered, this, [this] {
        if (!m_updateTool->idle())
            return;

        m_updateInfo.source = InternetSource;
        updateUi();
        checkForUpdates();
    });

    connect(m_updateSourceActions[LocalSource], &QAction::triggered, this, [this] {
        if (!m_updateTool->idle())
            return;

        QString fileName = QnFileDialog::getOpenFileName(this, tr("Select Update File..."), QString(), tr("Update Files (*.zip)"), 0, QnCustomFileDialog::fileDialogOptions());
        if (fileName.isEmpty()) {
            updateUi();
            return;
        }

        m_updateInfo.source = LocalSource;
        m_updateInfo.filename = fileName;

        updateUi();
        checkForUpdates();
    });


    connect(m_updateSourceActions[SpecificBuildSource], &QAction::triggered, this, [this] {
        if (!m_updateTool->idle())
            return;

        QnBuildNumberDialog dialog(this);
        if (!dialog.exec()) {
            updateUi();
            return;
        }

        m_updateInfo.source = SpecificBuildSource;
        QnSoftwareVersion version = qnCommon->engineVersion();
        m_updateInfo.build = QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber());

        updateUi();
        checkForUpdates();
    });


    ui->sourceButton->setIcon(qnSkin->icon("tree/branch_open.png"));
    connect(ui->sourceButton, &QPushButton::clicked, this, [this, menu] {
        QPoint local = ui->sourceButton->geometry().bottomLeft();
        menu->popup(ui->sourceButton->mapToGlobal(local));
    });
}


bool QnServerUpdatesWidget::cancelUpdate() {
    if (m_updateTool->isUpdating())
        return m_updateTool->cancelUpdate();
    return true;
}

bool QnServerUpdatesWidget::isUpdating() const {
    return m_updateTool->isUpdating();
}

void QnServerUpdatesWidget::setTargets(const QSet<QUuid> &targets) {
    m_updatesModel->setTargets(targets);
    m_updateTool->setTargets(targets);
}

QnMediaServerUpdateTool *QnServerUpdatesWidget::updateTool() const {
    return m_updateTool;
}

void QnServerUpdatesWidget::updateFromSettings() {
    if (!m_updateTool->idle())
        return;

    m_updateTool->reset();
    m_updatesModel->setTargets(QSet<QUuid>());
    updateUi();
}

void QnServerUpdatesWidget::checkForUpdates() {
    if (!m_updateTool->idle())
        return;

    m_updateTool->setDenyMajorUpdates(m_updateInfo.source == SpecificBuildSource);
    switch (m_updateInfo.source) {
    case InternetSource:
        m_updateTool->checkForUpdates();
        break;
    case LocalSource:
        m_updateTool->checkForUpdates(m_updateInfo.filename);
        break;
    case  SpecificBuildSource:
        m_updateTool->checkForUpdates(m_updateInfo.build);
        break;
    default:
        Q_ASSERT(false); //should never get here
        break;
    }
    m_updatesModel->setTargets(m_updateTool->actualTargets());

}

void QnServerUpdatesWidget::updateUi() {
    m_updateSourceActions[m_updateInfo.source]->setChecked(true);
    ui->sourceButton->setEnabled(m_updateTool->idle());
    for (QAction* action: m_updateSourceActions)
        action->setEnabled(m_updateTool->idle());

    foreach (const QnMediaServerResourcePtr &server, m_updateTool->actualTargets())
        m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(server->getId()));
    m_updatesModel->setLatestVersion(m_updateTool->targetVersion());
    if (!m_updateTool->targetVersion().isNull())
        ui->latestVersionLabel->setText(m_updateTool->targetVersion().toString());

    ui->startUpdateButton->setVisible(m_updateTool->canStartUpdate() ||
        m_updateTool->isUpdating());
    ui->startUpdateButton->setEnabled(m_updateTool->canStartUpdate());

    ui->dayWarningLabel->setVisible(QDateTime::currentDateTime().date().dayOfWeek() >= tooLateDayOfWeek 
        && m_updateTool->canStartUpdate());

    ui->updateStateWidget->setVisible(m_updateTool->isUpdating());
    ui->infiniteProgressWidget->setVisible(!m_updateTool->idle()); 

    if (!m_updateTool->idle())
        ui->detailLabel->setPalette(this->palette());   /* Remove warning style. */

    if (m_updateTool->state() == QnMediaServerUpdateTool::InstallingUpdate) {
        m_extraMessageTimer->start();
    } else {
        ui->extraMessageLabel->hide();
        m_extraMessageTimer->stop();
    }

    bool cancellable = false;

    switch (m_updateTool->state()) {
    case QnMediaServerUpdateTool::CheckingForUpdates:
        ui->detailLabel->setText(tr("Checking for updates..."));
        break;
    case QnMediaServerUpdateTool::DownloadingUpdate:
        cancellable = true;
        ui->detailLabel->setText(tr("Downloading updates"));
        break;
    case QnMediaServerUpdateTool::InstallingClientUpdate:
        cancellable = true;
        ui->detailLabel->setText(tr("Installing client update"));
        break;
    case QnMediaServerUpdateTool::InstallingToIncompatiblePeers:
        cancellable = true;
        ui->detailLabel->setText(tr("Installing updates to incompatible servers"));
        break;
    case QnMediaServerUpdateTool::UploadingUpdate:
        cancellable = true;
        ui->detailLabel->setText(tr("Pushing updates to servers"));
        break;
    case QnMediaServerUpdateTool::InstallingUpdate:
        ui->detailLabel->setText(tr("Installing updates"));
        break;
    default:
        break;
    }

    ui->detailWidget->setVisible(!ui->detailLabel->text().isEmpty());
    ui->cancelButton->setEnabled(cancellable);
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

void QnServerUpdatesWidget::at_checkForUpdatesFinished(QnCheckForUpdateResult result) {
    QPalette detailPalette = this->palette();
    QString detail;

    switch (result) {
    case QnCheckForUpdateResult::UpdateFound:
        if (!m_updateTool->targetVersion().isNull() && m_updateTool->isClientRequiresInstaller())
            detail = tr("Newer version found. You will have to update the client manually using an installer.");
        break;
    case QnCheckForUpdateResult::InternetProblem:
        detail = tr("Cannot check for updates via the Internet. "\
            "Click <a href=%1>here</a> to download updates manually.")
            .arg(m_updateTool->generateUpdatePackageUrl().toString());
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
        detail = tr("Cannot update from this file: %1").arg(m_updateInfo.filename);
        setWarningStyle(&detailPalette);
        break;
    }
    ui->detailWidget->setVisible(!detail.isEmpty());
    ui->detailLabel->setText(detail);
    ui->detailLabel->setPalette(detailPalette);
}

void QnServerUpdatesWidget::at_updateFinished(QnUpdateResult result) {
    switch (result) {
    case QnUpdateResult::Successful:
        {
            QString message = tr("Update has been successfully finished.");
            message += lit("\n");
            if (m_updateTool->isClientRequiresInstaller())
                message += tr("Now you have to update the client to the newer version using an installer.");
            else
                message += tr("The client will be restarted to the updated version.");

            QMessageBox::information(this, tr("Update is successful"), message);

            if (!m_updateTool->isClientRequiresInstaller()) {
                if (!applauncher::restartClient(m_updateTool->targetVersion()) == applauncher::api::ResultType::ok) {
                    QMessageBox::critical(this,
                        tr("Launcher process is not found"),
                        tr("Cannot restart the client.\n"
                        "Please close the application and start it again using the shortcut in the start menu."));
                } else {
                    qApp->exit(0);
                    applauncher::scheduleProcessKill(QCoreApplication::applicationPid(), processTerminateTimeout);
                }
            }
            break;
        }
    case QnUpdateResult::Cancelled:
        QMessageBox::information(this, tr("Update cancelled"), tr("Update has been cancelled."));
        break;
    case QnUpdateResult::LockFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Someone has already started an update."));
        break;
    case QnUpdateResult::DownloadingFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not download updates."));
        break;
    case QnUpdateResult::UploadingFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not push updates to servers."));
        break;
    case QnUpdateResult::ClientInstallationFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not install an update to the client."));
        break;
    case QnUpdateResult::InstallationFailed:
    case QnUpdateResult::RestInstallationFailed:
        QMessageBox::critical(this, tr("Update failed"), tr("Could not install updates on one or more servers."));
        break;
    }
    checkForUpdates();
}
