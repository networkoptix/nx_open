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
#include <ui/dialogs/update_url_dialog.h>
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

    connect(ui->cancelButton,                       &QPushButton::clicked,      m_updateTool,   &QnMediaServerUpdateTool::cancelUpdate);

    connect(m_updateTool,       &QnMediaServerUpdateTool::stateChanged,         this,           &QnServerUpdatesWidget::updateUi);
    connect(m_updateTool,       &QnMediaServerUpdateTool::progressChanged,      ui->updateProgessBar,   &QProgressBar::setValue);
    connect(m_updateTool,       &QnMediaServerUpdateTool::peerChanged,          this,           [this](const QUuid &peerId) {
        m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(peerId));
    });

    m_extraMessageTimer->setInterval(longInstallWarningTimeout);
    connect(m_extraMessageTimer, &QTimer::timeout, this, [this] {
        if (m_updateTool->state() == QnMediaServerUpdateTool::InstallingUpdate)
            ui->extraMessageLabel->show();
    });

    m_previousToolState = m_updateTool->state();

    ui->progressWidget->setText(tr("Checking for updates..."));

    setWarningStyle(ui->dayWarningLabel);
    static_assert(tooLateDayOfWeek <= Qt::Sunday, "In case of future days order change.");
    ui->dayWarningLabel->setVisible(QDateTime::currentDateTime().date().dayOfWeek() >= tooLateDayOfWeek);

    
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


    bool checkingForUpdates = false;
    bool applying = false;
    bool cancellable = false;
    bool startUpdate = false;
    bool infiniteProgress = false;
    bool installing = false;

    switch (m_updateTool->state()) {
    case QnMediaServerUpdateTool::Idle:
        if (m_previousToolState == QnMediaServerUpdateTool::CheckingForUpdates) {
            switch (m_updateTool->updateCheckResult()) {
            case QnMediaServerUpdateTool::UpdateFound:
                if (!m_updateTool->targetVersion().isNull()) {
                    // null version means we've got here for the first time after the dialog has been showed
                    QString message = tr("Do you want to update your system to version %1?").arg(m_updateTool->targetVersion().toString());
                    if (m_updateTool->isClientRequiresInstaller()) {
                        message += lit("\n\n");
                        message += tr("You will have to update the client manually using an installer.");
                    }

                    startUpdate = QMessageBox::question(this, tr("Update is found"), message, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
                }
                break;
            case QnMediaServerUpdateTool::InternetProblem: {
                QnUpdateUrlDialog dialog;
                dialog.setUpdatesUrl(m_updateTool->generateUpdatePackageUrl().toString());
                dialog.exec();
                break;
                                                           }
            case QnMediaServerUpdateTool::NoNewerVersion:
                QMessageBox::information(this, tr("Update is not found"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::NoSuchBuild:
                QMessageBox::critical(this, tr("Wrong build number"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::UpdateImpossible:
                QMessageBox::critical(this, tr("Update is impossible"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::BadUpdateFile:
                QMessageBox::critical(this, tr("Update check failed"), m_updateTool->resultString());
                break;
            }
        } else if (m_previousToolState > QnMediaServerUpdateTool::CheckingForUpdates) {
            switch (m_updateTool->updateResult()) {
            case QnMediaServerUpdateTool::UpdateSuccessful:
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
            case QnMediaServerUpdateTool::Cancelled:
                QMessageBox::information(this, tr("Update cancelled"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::LockFailed:
            case QnMediaServerUpdateTool::DownloadingFailed:
            case QnMediaServerUpdateTool::UploadingFailed:
            case QnMediaServerUpdateTool::InstallationFailed:
            case QnMediaServerUpdateTool::RestInstallationFailed:
                QMessageBox::critical(this, tr("Update failed"), m_updateTool->resultString());
                break;
            }
        }
        break;
    case QnMediaServerUpdateTool::CheckingForUpdates:
        checkingForUpdates = true;
        break;
    case QnMediaServerUpdateTool::DownloadingUpdate:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Downloading updates"));
        break;
    case QnMediaServerUpdateTool::InstallingClientUpdate:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Installing client update"));
        infiniteProgress = true;
        break;
    case QnMediaServerUpdateTool::InstallingToIncompatiblePeers:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Installing updates to incompatible servers"));
        break;
    case QnMediaServerUpdateTool::UploadingUpdate:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Uploading updates to servers"));
        break;
    case QnMediaServerUpdateTool::InstallingUpdate:
        applying = true;
        infiniteProgress = true;
        installing = true;
        ui->updateStateLabel->setText(tr("Installing updates"));
        break;
    default:
        break;
    }

    ui->cancelButton->setVisible(applying);
    ui->cancelButton->setEnabled(cancellable);
    ui->updateStateWidget->setVisible(applying);
    ui->progressWidget->setVisible(checkingForUpdates);
    ui->progressIndicator->setVisible(infiniteProgress);
    ui->updateProgessBar->setVisible(!infiniteProgress);


    m_previousToolState = m_updateTool->state();

    if (installing) {
        m_extraMessageTimer->start();
    } else {
        ui->extraMessageLabel->hide();
        m_extraMessageTimer->stop();
    }

    if (startUpdate)
        m_updateTool->updateServers();
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
