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

#include <utils/common/software_version.h>
#include <utils/media_server_update_tool.h>
#include <utils/applauncher_utils.h>

#include <version.h>

namespace {
    const int longInstallWarningTimeout = 2 * 60 * 1000; // 2 minutes
    // Time that is given to process to exit. After that, appauncher (if present) will try to terminate it.
    static const quint32 processTerminateTimeout = 15000;
}

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnServerUpdatesWidget),
    m_minimalMode(false),
    m_extraMessageTimer(new QTimer(this))
{
    ui->setupUi(this);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(this);

    QnSortedServerUpdatesModel *sortedUpdatesModel = new QnSortedServerUpdatesModel(this);
    sortedUpdatesModel->setSourceModel(m_updatesModel);
    sortedUpdatesModel->sort(0); // the column does not matter because the model uses column-independent sorting

    ui->tableView->setModel(sortedUpdatesModel);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::ResourceNameColumn, QHeaderView::Stretch);
    ui->tableView->setItemDelegateForColumn(QnServerUpdatesModel::UpdateColumn, new QnUpdateStatusItemDelegate(ui->tableView));

    connect(ui->updateFromLocalSourceButton,        &QPushButton::clicked,      this,           &QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked);
    connect(ui->checkForUpdatesButton,              &QPushButton::clicked,      this,           &QnServerUpdatesWidget::at_checkForUpdatesButton_clicked);
    connect(ui->installSpecificBuildButton,         &QPushButton::clicked,      this,           &QnServerUpdatesWidget::at_installSpecificBuildButton_clicked);
    connect(ui->cancelButton,                       &QPushButton::clicked,      m_updateTool,   &QnMediaServerUpdateTool::cancelUpdate);

    connect(m_updateTool,       &QnMediaServerUpdateTool::stateChanged,         this,           &QnServerUpdatesWidget::updateUi);
    connect(m_updateTool,       &QnMediaServerUpdateTool::progressChanged,      ui->updateProgessBar,   &QProgressBar::setValue);
    connect(m_updateTool,       &QnMediaServerUpdateTool::peerChanged,          this,           &QnServerUpdatesWidget::at_updateTool_peerChanged);

    m_extraMessageTimer->setInterval(longInstallWarningTimeout);
    connect(m_extraMessageTimer, &QTimer::timeout, this, &QnServerUpdatesWidget::at_extraMessageTimer_timeout);

    m_previousToolState = m_updateTool->state();

    ui->progressWidget->setText(tr("Checking for updates..."));
    updateUi();
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

bool QnServerUpdatesWidget::isMinimalMode() const {
    return m_minimalMode;
}

void QnServerUpdatesWidget::setMinimalMode(bool minimalMode) {
    m_minimalMode = minimalMode;
    ui->topButtonBar->setVisible(!m_minimalMode);
}

void QnServerUpdatesWidget::updateFromSettings() {
    if (m_updateTool->state() != QnMediaServerUpdateTool::Idle)
        return;

    m_updateTool->reset();
    m_updatesModel->setTargets(QSet<QUuid>());
    updateUi();
}

void QnServerUpdatesWidget::checkForUpdates() {
    m_updateTool->setDenyMajorUpdates(false);
    m_updateTool->checkForUpdates();
    m_updatesModel->setTargets(m_updateTool->actualTargets());
}

void QnServerUpdatesWidget::at_checkForUpdatesButton_clicked() {
    checkForUpdates();
}

void QnServerUpdatesWidget::at_installSpecificBuildButton_clicked() {
    QnBuildNumberDialog dialog(this);
    if (dialog.exec() == QDialog::Rejected)
        return;

    m_updateTool->setDenyMajorUpdates(true);
    QnSoftwareVersion version = qnCommon->engineVersion();
    m_updateTool->checkForUpdates(QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber()));
    m_updatesModel->setTargets(m_updateTool->actualTargets());
}

void QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked() {
    QString fileName = QnFileDialog::getOpenFileName(this, tr("Select Update File..."), QString(), tr("Update Files (*.zip)"), 0, QnCustomFileDialog::fileDialogOptions());
    if (fileName.isEmpty())
        return;

    m_updateTool->setDenyMajorUpdates(false);
    m_updateTool->checkForUpdates(fileName);
    m_updatesModel->setTargets(m_updateTool->actualTargets());
}

void QnServerUpdatesWidget::at_updateButton_clicked() {
    bool haveOffline = false;
    foreach (const QnMediaServerResourcePtr &resource, m_updateTool->actualTargets()) {
        if (resource->getStatus() == Qn::Offline) {
            haveOffline = true;
            break;
        }
    }

    if (haveOffline) {
        QMessageBox::warning(this, tr("Warning"),
                              tr("Some servers in your system are offline now.\n"
                                 "They will be not upgraded now."));
    }

    m_updateTool->updateServers();
}

void QnServerUpdatesWidget::at_updateTool_peerChanged(const QUuid &peerId) {
    m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(peerId));
}

void QnServerUpdatesWidget::at_extraMessageTimer_timeout() {
    if (m_updateTool->state() == QnMediaServerUpdateTool::InstallingUpdate)
        ui->extraMessageLabel->show();
}

void QnServerUpdatesWidget::updateUi() {
    bool checkingForUpdates = false;
    bool applying = false;
    bool cancellable = false;
    bool startUpdate = false;
    bool infiniteProgress = false;
    bool installing = false;

    foreach (const QnMediaServerResourcePtr &server, m_updateTool->actualTargets())
        m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(server->getId()));

    switch (m_updateTool->state()) {
    case QnMediaServerUpdateTool::Idle:
        if (m_previousToolState == QnMediaServerUpdateTool::CheckingForUpdates) {
            switch (m_updateTool->updateCheckResult()) {
            case QnMediaServerUpdateTool::UpdateFound:
                if (!m_updateTool->targetVersion().isNull() && !m_minimalMode) {
                    // null version means we've got here for the first time after the dialog has been showed
                    QString message = tr("Do you want to update your system to version %1?").arg(m_updateTool->targetVersion().toString());
                    if (m_updateTool->isClientRequiresInstaller()) {
                        message += lit("\n\n");
                        message += tr("You will have to update the client manually using an installer.");
                    }
                    switch (QDateTime::currentDateTime().date().dayOfWeek()) {
                    case Qt::Thursday:
                    case Qt::Friday:
                    case Qt::Saturday:
                    case Qt::Sunday:
                        message += lit("\n\n");
                        message += tr("As a general rule for the sake of better support, we do not recommend to make system updates at the end of the week.");
                        break;
                    default:
                        break;
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
                if (!m_minimalMode)
                    QMessageBox::information(this, tr("Update is not found"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::NoSuchBuild:
                if (!m_minimalMode)
                    QMessageBox::critical(this, tr("Wrong build number"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::UpdateImpossible:
                if (!m_minimalMode)
                    QMessageBox::critical(this, tr("Update is impossible"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::BadUpdateFile:
                if (!m_minimalMode)
                    QMessageBox::critical(this, tr("Update check failed"), m_updateTool->resultString());
                break;
            }
        } else if (m_previousToolState > QnMediaServerUpdateTool::CheckingForUpdates) {
            switch (m_updateTool->updateResult()) {
            case QnMediaServerUpdateTool::UpdateSuccessful:
                if (!m_minimalMode) {
                    QString message = tr("Update has been successfully finished.");
                    message += lit("\n");
                    if (m_updateTool->isClientRequiresInstaller())
                        message += tr("Now you have to update the client to the newer version using an installer.");
                    else
                        message += tr("The client will be restarted to the updated version.");

                    QMessageBox::information(this, tr("Update is successfull"), message);

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
                }
                break;
            case QnMediaServerUpdateTool::Cancelled:
                if (!m_minimalMode)
                    QMessageBox::information(this, tr("Update cancelled"), m_updateTool->resultString());
                break;
            case QnMediaServerUpdateTool::LockFailed:
            case QnMediaServerUpdateTool::DownloadingFailed:
            case QnMediaServerUpdateTool::UploadingFailed:
            case QnMediaServerUpdateTool::InstallationFailed:
            case QnMediaServerUpdateTool::RestInstallationFailed:
                if (!m_minimalMode)
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
        ui->updateStateLabel->setText(tr("Installing udates to incompatible servers"));
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
    ui->checkForUpdatesButton->setEnabled(!applying && !checkingForUpdates);
    ui->installSpecificBuildButton->setEnabled(!applying && !checkingForUpdates);
    ui->updateFromLocalSourceButton->setEnabled(!applying && !checkingForUpdates);

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
