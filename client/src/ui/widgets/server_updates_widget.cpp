#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtWidgets/QMessageBox>

#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <ui/models/sorted_server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <ui/delegates/update_status_item_delegate.h>
#include <utils/media_server_update_tool.h>

#include <version.h>

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnServerUpdatesWidget)
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
    updateUi();
}

QnServerUpdatesWidget::~QnServerUpdatesWidget() {}

bool QnServerUpdatesWidget::cancelUpdate() {
    if (m_updateTool->isUpdating())
        return m_updateTool->cancelUpdate();
    return true;
}

bool QnServerUpdatesWidget::isUpdating() const {
    return m_updateTool->isUpdating();
}

void QnServerUpdatesWidget::setTargets(const QSet<QnId> &targets) {
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

void QnServerUpdatesWidget::at_checkForUpdatesButton_clicked() {
    m_updateTool->setDenyMajorUpdates(false);
    m_updateTool->checkForUpdates();
}

void QnServerUpdatesWidget::at_installSpecificBuildButton_clicked() {
    QnBuildNumberDialog dialog(this);
    if (dialog.exec() == QDialog::Rejected)
        return;

    m_updateTool->setDenyMajorUpdates(true);
    QnSoftwareVersion version = qnCommon->engineVersion();
    m_updateTool->checkForUpdates(QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber()));
}

void QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked() {
    QString sourceDir = QnFileDialog::getExistingDirectory(this, tr("Select updates folder..."), QString(), QnCustomFileDialog::directoryDialogOptions());
    if (sourceDir.isEmpty())
        return;

    m_updateTool->setDenyMajorUpdates(false);
    m_updateTool->checkForUpdates(sourceDir);
}

void QnServerUpdatesWidget::at_updateButton_clicked() {
    bool haveOffline = false;
    foreach (const QnMediaServerResourcePtr &resource, m_updateTool->actualTargets()) {
        if (resource->getStatus() == QnResource::Offline) {
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

void QnServerUpdatesWidget::at_updateTool_peerChanged(const QnId &peerId) {
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
                if (!m_updateTool->targetVersion().isNull() && !m_minimalMode) { // null version means we've got here first time after the dialog has been showed
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
            }
        } else {
            switch (m_updateTool->updateResult()) {
            case QnMediaServerUpdateTool::UpdateSuccessful:
                if (!m_minimalMode) {
                    QMessageBox::information(this,
                                             tr("Update is successfull"),
                                             tr("Update has been successfully finished.\n"
                                                "Client will be restarted and updated."));
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
        ui->updateStateLabel->setText(tr("Installing updates"));
        break;
    default:
        break;
    }

    foreach (const QnMediaServerResourcePtr &server, m_updateTool->actualTargets())
        m_updatesModel->setUpdateInformation(m_updateTool->updateInformation(server->getId()));

    if (checkingForUpdates)
        ui->buttonBox->showProgress(tr("Checking for updates..."));
    else
        ui->buttonBox->hideProgress();

    ui->cancelButton->setVisible(applying);
    ui->cancelButton->setEnabled(cancellable);
    ui->updateStateWidget->setVisible(applying);

    m_previousToolState = m_updateTool->state();

    if (startUpdate)
        m_updateTool->updateServers();
}

void QnServerUpdatesWidget::createUpdatesDownloader() {

}
