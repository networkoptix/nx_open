#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtWidgets/QMessageBox>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>

#include <utils/common/software_version.h>
#include <utils/media_server_update_tool.h>

#include <version.h>

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnServerUpdatesWidget),
    m_specificBuildCheck(false)
{
    ui->setupUi(this);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(this);

    QnSortedServerUpdatesModel *model = new QnSortedServerUpdatesModel(this);
    model->setSourceModel(m_updatesModel);
    model->sort(0); // the column does not matter because the model uses column-independent sorting

    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::ResourceNameColumn, QHeaderView::Stretch);
    // TODO: #dklychkov fix progress bar painting and uncomment the line below
//    ui->tableView->setItemDelegateForColumn(QnServerUpdatesModel::UpdateColumn, new QnUpdateStatusItemDelegate(ui->tableView));

    connect(ui->updateFromLocalSourceButton,        &QPushButton::clicked,      this,           &QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked);
    connect(ui->checkForUpdatesButton,              &QPushButton::clicked,      this,           &QnServerUpdatesWidget::at_checkForUpdatesButton_clicked);
    connect(ui->installSpecificBuildButton,         &QPushButton::clicked,      this,           &QnServerUpdatesWidget::at_installSpecificBuildButton_clicked);
    connect(ui->cancelButton,                       &QPushButton::clicked,      m_updateTool,   &QnMediaServerUpdateTool::cancelUpdate);

    connect(m_updateTool,       &QnMediaServerUpdateTool::stateChanged,         this,           &QnServerUpdatesWidget::updateUi);
    connect(m_updateTool,       &QnMediaServerUpdateTool::progressChanged,      ui->updateProgessBar,   &QProgressBar::setValue);
    connect(m_updateTool,       &QnMediaServerUpdateTool::peerChanged,          this,           &QnServerUpdatesWidget::at_updateTool_peerChanged);

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

void QnServerUpdatesWidget::at_checkForUpdatesButton_clicked() {
    m_updateTool->checkForUpdates();
}

void QnServerUpdatesWidget::at_installSpecificBuildButton_clicked() {
    QnBuildNumberDialog dialog(this);
    if (dialog.exec() == QDialog::Rejected)
        return;

    m_specificBuildCheck = true;
    QnSoftwareVersion version(lit(QN_APPLICATION_VERSION));
    m_updateTool->checkForUpdates(QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber()));
}

void QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked() {
    QString sourceDir = QnFileDialog::getExistingDirectory(this, tr("Select updates folder..."), QString(), QnCustomFileDialog::directoryDialogOptions());
    if (sourceDir.isEmpty())
        return;

    m_updateTool->checkForUpdates(sourceDir);
}

void QnServerUpdatesWidget::at_updateButton_clicked() {
    bool haveOffline = false;
    QList<QnMediaServerResourcePtr> servers = m_updatesModel->servers();
    foreach (const QnMediaServerResourcePtr &resource, servers) {
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

void QnServerUpdatesWidget::updateUi() {
    bool checkingForUpdates = false;
    bool applying = false;
    bool cancellable = false;
    bool startUpdate = false;

    switch (m_updateTool->state()) {
    case QnMediaServerUpdateTool::Idle:
        if (m_previousToolState <= QnMediaServerUpdateTool::CheckingForUpdates) {
            switch (m_updateTool->updateCheckResult()) {
            case QnMediaServerUpdateTool::UpdateFound:
                if (m_specificBuildCheck) {
                    m_specificBuildCheck = false;

                    QnSoftwareVersion currentVersion(QN_APPLICATION_VERSION);
                    QnSoftwareVersion foundVersion = m_updateTool->targetVersion();

                    if (currentVersion.major() != foundVersion.major() || currentVersion.minor() != foundVersion.minor()) {
                        QMessageBox::critical(this, tr("Wrong build number"), tr("There is no such build on the update server"));
                        break;
                    }
                }

                if (!m_updateTool->targetVersion().isNull()) { // null version means we've got here first time after the dialog has been showed
                    int result = QMessageBox::question(this, tr("Update is found"),
                                                       tr("Do you want to update your system to version %1?").arg(m_updateTool->targetVersion().toString()),
                                                       QMessageBox::Yes | QMessageBox::No);
                    startUpdate = (result == QMessageBox::Yes);
                }
                break;
            case QnMediaServerUpdateTool::InternetProblem: {
                int result = QMessageBox::warning(this,
                                                  tr("Check for updates failed"),
                                                  tr("Cannot check for updates via the Internet.\n"
                                                     "Do you want to create a download helper to download updates on the computer which is connected to the internet?"),
                                                  QMessageBox::Yes, QMessageBox::No);
                if (result == QMessageBox::Yes)
                    createUpdatesDownloader();

                break;
            }
            case QnMediaServerUpdateTool::NoNewerVersion:
                QMessageBox::information(this, tr("Update is not found"), tr("All component in your system are already up to date."));
                break;
            case QnMediaServerUpdateTool::NoSuchBuild:
                QMessageBox::critical(this, tr("Wrong build number"), tr("There is no such build on the update server"));
                break;
            case QnMediaServerUpdateTool::UpdateImpossible:
                QMessageBox::critical(this, tr("Update is impossible"), tr("Cannot start update.\nUpdate for one or more servers were not found."));
                break;
            }
        } else {
            switch (m_updateTool->updateResult()) {
            case QnMediaServerUpdateTool::UpdateSuccessful:
                QMessageBox::information(this,
                                         tr("Update is successful"),
                                         tr("Update has been successfully finished.\n"
                                            "Client will be restarted and updated."));
                break;
            case QnMediaServerUpdateTool::Cancelled:
                QMessageBox::information(this,
                                         tr("Update cancelled"),
                                         tr("Update has been cancelled."));
                break;
            case QnMediaServerUpdateTool::LockFailed:
                QMessageBox::critical(this,
                                      tr("Update failed"),
                                      tr("Someone has already started an update."));
                break;
            case QnMediaServerUpdateTool::DownloadingFailed:
                QMessageBox::critical(this,
                                      tr("Update failed"),
                                      tr("Could not download updates."));
                break;
            case QnMediaServerUpdateTool::UploadingFailed:
                QMessageBox::critical(this,
                                      tr("Update failed"),
                                      tr("Could not upload updates to servers."));
                break;
            case QnMediaServerUpdateTool::InstallationFailed:
                QMessageBox::critical(this,
                                      tr("Update failed"),
                                      tr("Could not install updates on one or more servers."));
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
    case QnMediaServerUpdateTool::UploadingUpdate:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Uploading updates to servers"));
        break;
    case QnMediaServerUpdateTool::InstallingUpdate:
        applying = true;
        ui->updateStateLabel->setText(tr("Installing updates"));
        break;
    default:
        break;
    }

    foreach (const QnMediaServerResourcePtr &server, m_updatesModel->servers())
        m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(server->getId()));

    ui->progressWidget->setVisible(checkingForUpdates);

    ui->cancelButton->setVisible(applying);
    ui->cancelButton->setEnabled(cancellable);
    ui->updateStateWidget->setVisible(applying);

    m_previousToolState = m_updateTool->state();

    if (startUpdate)
        m_updateTool->updateServers();
}

void QnServerUpdatesWidget::createUpdatesDownloader() {

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
