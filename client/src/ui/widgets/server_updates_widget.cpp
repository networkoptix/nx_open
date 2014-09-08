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

#include <update/media_server_update_tool.h>

#include <utils/applauncher_utils.h>

#include <version.h>

namespace {
    const int longInstallWarningTimeout = 2 * 60 * 1000; // 2 minutes
    // Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
    const quint32 processTerminateTimeout = 15000;

    const int tooLateDayOfWeek = Qt::Thursday;

    const int autoCheckIntervalMs = 1 * 60 * 1000;  // 5 minutes

    const QString copyLinkAction = lit("copyLink");
    const QString saveLinkAction = lit("saveLink");
}

QnServerUpdatesWidget::QnServerUpdatesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnServerUpdatesWidget),
    m_extraMessageTimer(new QTimer(this)),
    m_checkingInternet(false),
    m_checkingLocal(false)
{
    ui->setupUi(this);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(m_updateTool, this);

    QnSortedServerUpdatesModel *sortedUpdatesModel = new QnSortedServerUpdatesModel(this);
    sortedUpdatesModel->setSourceModel(m_updatesModel);
    sortedUpdatesModel->sort(0); // the column does not matter because the model uses column-independent sorting

    ui->tableView->setModel(sortedUpdatesModel);
    ui->tableView->setItemDelegateForColumn(QnServerUpdatesModel::VersionColumn, new QnUpdateStatusItemDelegate(ui->tableView));

    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::NameColumn, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::VersionColumn, QHeaderView::ResizeToContents);   

    connect(ui->cancelButton,           &QPushButton::clicked,      m_updateTool, &QnMediaServerUpdateTool::cancelUpdate);
    connect(ui->internetUpdateButton,   &QPushButton::clicked,      this, [this] {
        m_updateTool->startUpdate(m_targetVersion, !m_targetVersion.isNull());
    });

    connect(ui->filenameLineEdit,   &QLineEdit::textChanged,    this, [this](const QString &text) {
        ui->localUpdateButton->setEnabled(!text.isEmpty());
    });
    ui->localUpdateButton->setEnabled(false);
    connect(ui->localUpdateButton,      &QPushButton::clicked,      this, [this] {
        m_updateTool->startUpdate(ui->filenameLineEdit->text());
    });

    connect(ui->internetRadioButton,    &QRadioButton::toggled,     this, [this] {
        ui->sourceWidget->setCurrentWidget(ui->internetPage);
    });

    connect(ui->localRadioButton,       &QRadioButton::toggled,     this, [this] {
        ui->sourceWidget->setCurrentWidget(ui->localPage);
    });

    connect(ui->refreshButton,          &QPushButton::clicked,      this, [this] {
        checkForUpdatesInternet();
    });

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

    initLinkButtons();
    initBuildSelectionButtons();

    connect(m_updateTool,       &QnMediaServerUpdateTool::stageChanged,         this,           &QnServerUpdatesWidget::at_tool_stageChanged);
    connect(m_updateTool,       &QnMediaServerUpdateTool::progressChanged,      ui->updateProgessBar,   &QProgressBar::setValue);
    connect(m_updateTool,       &QnMediaServerUpdateTool::updateFinished,           this, &QnServerUpdatesWidget::at_updateFinished);

    m_extraMessageTimer->setInterval(longInstallWarningTimeout);
    connect(m_extraMessageTimer, &QTimer::timeout, this, [this] {
        if (m_updateTool->stage() == QnFullUpdateStage::Servers)
            ui->extraMessageLabel->show();
    });

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

        checkForUpdatesInternet();
    });
    updateTimer->start();

    at_tool_stageChanged(QnFullUpdateStage::Init);
    checkForUpdatesInternet(true);
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
    });

    ui->copyLinkButton->setEnabled(false);
    ui->saveLinkButton->setEnabled(false);
}

void QnServerUpdatesWidget::initBuildSelectionButtons() {
    connect(ui->latestBuildLabel,   &QLabel::linkActivated, this, [this] {
        m_targetVersion = QnSoftwareVersion();
        ui->linkLineEdit->setText(m_updateTool->generateUpdatePackageUrl(m_targetVersion).toString());
        ui->latestBuildLabel->setVisible(false);
        ui->specificBuildLabel->setVisible(true);

        if (m_latestVersion.isNull())
            ui->latestVersionLabel->setText(tr("Unknown"));
        else
            ui->latestVersionLabel->setText(m_latestVersion.toString());

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

        checkForUpdatesInternet(true);
    });

    ui->latestBuildLabel->setVisible(false);
}

bool QnServerUpdatesWidget::canStartUpdate() {
    return m_updateTool->idle(); //TODO: #GDM implement
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
    m_updateTool->setTargets(targets);
}

QnMediaServerUpdateTool *QnServerUpdatesWidget::updateTool() const {
    return m_updateTool;
}

void QnServerUpdatesWidget::updateFromSettings() {
    if (!m_updateTool->idle())
        return;

    ui->internetRadioButton->setChecked(true);
    checkForUpdatesInternet();
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

void QnServerUpdatesWidget::at_updateFinished(QnUpdateResult result) {
 /*   switch (result) {
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
    checkForUpdates();*/
}

void QnServerUpdatesWidget::checkForUpdatesInternet(bool autoSwitch) {
    if (m_checkingInternet || !m_updateTool->idle())
        false;
    m_checkingInternet = true;

    auto updateUi = [this](bool enabled) {
        ui->refreshButton->setEnabled(enabled);
        ui->internetUpdateButton->setEnabled(enabled);
        ui->latestBuildLabel->setEnabled(enabled);
        ui->specificBuildLabel->setEnabled(enabled);
        ui->internetInfiniteProgress->setVisible(!enabled);
    };
    updateUi(false);
    ui->internetDetailLabel->setText(tr("Checking for updates..."));
    ui->internetDetailLabel->setVisible(true);

    QnSoftwareVersion targetVersion = m_targetVersion;

    m_updateTool->checkForUpdates(m_targetVersion, !m_targetVersion.isNull(), [this, autoSwitch, updateUi, targetVersion](const QnCheckForUpdateResult &result) {
        QPalette detailPalette = this->palette();
        QPalette statusPalette = this->palette();
        QString detail;
        QString status;

        switch (result.result) {
        case QnCheckForUpdateResult::UpdateFound:
            /*
            if (result.clientInstallerRequired)
                detail = tr("Newer version found. You will have to update the client manually using an installer.");
             */
            status = result.latestVersion.toString();
            break;
        case QnCheckForUpdateResult::NoNewerVersion:
            status = result.latestVersion.toString();
            break;
        case QnCheckForUpdateResult::InternetProblem:
            status = tr("Internet connection problem");
            setWarningStyle(&statusPalette);
            break;
        case QnCheckForUpdateResult::NoSuchBuild:
            status = m_targetVersion.toString();
            detail = tr("There is no such build on the update server");
            setWarningStyle(&statusPalette);
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ServerUpdateImpossible:
            status = result.latestVersion.toString();
            detail = tr("Cannot start update. An update for one or more servers was not found.");
            setWarningStyle(&detailPalette);
            break;
        case QnCheckForUpdateResult::ClientUpdateImpossible:
            status = result.latestVersion.toString();
            detail = tr("Cannot start update. An update for the client was not found.");
            setWarningStyle(&detailPalette);
            break;
        default:
            Q_ASSERT(false);    //should never get here
        }

        /* Update latest version if we have checked for updates in the internet. */
        if (targetVersion.isNull() && !result.latestVersion.isNull())
            m_latestVersion = result.latestVersion;

        if (autoSwitch && result.result == QnCheckForUpdateResult::InternetProblem) {
            ui->localRadioButton->setChecked(true);
        }
        ui->connectionProblemLabel->setVisible(result.result == QnCheckForUpdateResult::InternetProblem);

        ui->internetDetailLabel->setText(detail);
        ui->internetDetailLabel->setPalette(detailPalette);
        ui->latestVersionLabel->setText(status);
        ui->latestVersionLabel->setPalette(statusPalette);

        m_checkingInternet = false;
        updateUi(true);
    });

}

void QnServerUpdatesWidget::checkForUpdatesLocal() {
    if (m_checkingLocal)
        false;
    m_checkingLocal = true;

    m_updateTool->checkForUpdates(ui->filenameLineEdit->text(), [this](const QnCheckForUpdateResult &result){


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
            detail = tr("Cannot update from this file");
            setWarningStyle(&detailPalette);
            break;
        default:
            Q_ASSERT(false);    //should never get here
        }

        ui->localDetailLabel->setText(detail);
        ui->localDetailLabel->setPalette(detailPalette);

        m_checkingLocal = false;
    });
}

void QnServerUpdatesWidget::at_tool_stageChanged(QnFullUpdateStage stage) {
    
/*    ui->sourceButton->setEnabled(m_updateTool->idle());*/
/*
    for (QAction* action: m_updateSourceActions)
        action->setEnabled(m_updateTool->idle());
*/

  /*  if (!m_updateTool->targetVersion().isNull())
        ui->latestVersionLabel->setText(m_updateTool->targetVersion().toString());*/

   /* ui->startUpdateButton->setVisible(canStartUpdate() ||
        m_updateTool->isUpdating());
    ui->startUpdateButton->setEnabled(canStartUpdate());
*/

    ui->dayWarningLabel->setVisible(QDateTime::currentDateTime().date().dayOfWeek() >= tooLateDayOfWeek 
        && canStartUpdate());

    ui->updateStateWidget->setVisible(stage != QnFullUpdateStage::Init);

    if (stage == QnFullUpdateStage::Servers) {
        m_extraMessageTimer->start();
    } else {
        ui->extraMessageLabel->hide();
        m_extraMessageTimer->stop();
    }

    bool cancellable = false;

    switch (stage) {
    case QnFullUpdateStage::Check:
        ui->updateProgessBar->setFormat(tr("Checking for updates... %p%"));
        break;
    case QnFullUpdateStage::Download:
        cancellable = true;
        ui->updateProgessBar->setFormat(tr("Downloading updates... %p%"));
        break;
    case QnFullUpdateStage::Client:
        cancellable = true;
        ui->updateProgessBar->setFormat(tr("Installing client update... %p%"));
        break;
    case QnFullUpdateStage::Incompatible:
        cancellable = true;
        ui->updateProgessBar->setFormat(tr("Installing updates to incompatible servers... %p%"));
        break;
    case QnFullUpdateStage::Push:
        cancellable = true;
        ui->updateProgessBar->setFormat(tr("Pushing updates to servers... %p%"));
        break;
    case QnFullUpdateStage::Servers:
        ui->updateProgessBar->setFormat(tr("Installing updates... %p%"));
        break;
    default:
        break;
    }

    ui->cancelButton->setEnabled(cancellable);
}

