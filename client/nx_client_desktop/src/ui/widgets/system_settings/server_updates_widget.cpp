#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

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
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
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

#include <update/media_server_update_tool.h>
#include <update/low_free_space_warning.h>

#include <utils/applauncher_utils.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/connection_diagnostics_helper.h>

using namespace nx::client::desktop::ui;

namespace {

const int kLongInstallWarningTimeoutMs = 2 * 60 * 1000; // 2 minutes
// Time that is given to process to exit. After that, applauncher (if present) will try to terminate it.
const quint32 kProcessTerminateTimeoutMs = 15000;

const int kTooLateDayOfWeek = Qt::Thursday;

const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour

const int kMaxLabelWidth = 400; // pixels

const int kVersionLabelFontSizePixels = 24;
const int kVersionLabelFontWeight = QFont::DemiBold;

constexpr auto kLatestVersionBannerLabelFontSizePixels = 22;
constexpr auto kLatestVersionBannerLabelFontWeight = QFont::Light;

const int kLinkCopiedMessageTimeoutMs = 2000;

/* N-dash 5 times: */
const QString kNoVersionNumberText = QString::fromWCharArray(L"\x2013\x2013\x2013\x2013\x2013");

QString elidedText(const QString& text, const QFontMetrics& fontMetrics)
{
    QString result;

    for (const auto& line : text.split(L'\n', QString::KeepEmptyParts))
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
    m_longUpdateWarningTimer(new QTimer(this))
{
    ui->setupUi(this);

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
    ui->linkCopiedIconLabel->setPixmap(qnSkin->pixmap("buttons/checkmark.png"));
    ui->linkCopiedWidget->hide();

    setHelpTopic(this, Qn::Administration_Update_Help);

    m_updateTool = new QnMediaServerUpdateTool(this);
    m_updatesModel = new QnServerUpdatesModel(m_updateTool, this);
    connect(m_updatesModel, &QnServerUpdatesModel::lowestInstalledVersionChanged, this,
        &QnServerUpdatesWidget::updateVersionPage);

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
        &QnServerUpdatesWidget::cancelUpdate);

    connect(ui->updateButton, &QPushButton::clicked, this,
        [this]
        {
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
    ui->releaseDescriptionLabel->setText(QString());
    ui->releaseDescriptionLabel->setVisible(false);

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

    updateButtonText();
    updateButtonAccent();
    updateDownloadButton();
    updateVersionPage();
}

QnServerUpdatesWidget::~QnServerUpdatesWidget()
{
    /* When QnServerUpdatesWidget gets to QObject destructo it destroys m_updateTool.
       QnMediaServerUpdateTool stops in destructor and emits state change signals
       which cannot be handled in already destroyed QnServerUpdatesWidget.
       Also there's no need to handle them, so just disconnect. */
    m_updateTool->disconnect(this);
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

void QnServerUpdatesWidget::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    updateButtonText();
    updateButtonAccent();
    updateDownloadButton();
    updateVersionPage();
}

void QnServerUpdatesWidget::initDropdownActions()
{
    auto selectUpdateTypeMenu = new QMenu(this);
    selectUpdateTypeMenu->setProperty(style::Properties::kMenuAsDropdown, true);

    auto defaultAction = selectUpdateTypeMenu->addAction(tr("Latest Available Update"),
        [this]()
        {
            setMode(Mode::LatestVersion);
            m_targetVersion = QnSoftwareVersion();
            m_targetChangeset = QString();
            m_localFileName = QString();
            m_updatesModel->setLatestVersion(m_latestVersion);

            ui->selectUpdateTypeButton->setText(tr("Latest Available Update"));
            ui->targetVersionLabel->setText(m_latestVersion.isNull()
                ? kNoVersionNumberText
                : m_latestVersion.toString());

            checkForUpdates(true);
        });

    selectUpdateTypeMenu->addAction(tr("Specific Build..."),
        [this]()
        {
            QnBuildNumberDialog dialog(this);
            if (!dialog.exec())
                return;

            setMode(Mode::SpecificBuild);
            QnSoftwareVersion version = qnStaticCommon->engineVersion();
            m_targetVersion = QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber());
            m_targetChangeset = dialog.changeset();
            m_localFileName = QString();
            m_updatesModel->setLatestVersion(m_targetVersion);

            ui->targetVersionLabel->setText(m_targetVersion.toString());
            ui->selectUpdateTypeButton->setText(tr("Selected Version"));

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

            setMode(Mode::LocalFile);
            ui->targetVersionLabel->setText(kNoVersionNumberText);
            ui->selectUpdateTypeButton->setText(tr("Selected Update File"));
            m_updatesModel->setLatestVersion(QnSoftwareVersion());

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
                m_updateTool->generateUpdatePackageUrl(m_targetVersion, m_targetChangeset));
        });

    downloadLinkMenu->addAction(tr("Copy Link to Clipboard"),
        [this]()
        {
            qApp->clipboard()->setText(
                m_updateTool->generateUpdatePackageUrl(m_targetVersion, m_targetChangeset).toString());

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

void QnServerUpdatesWidget::updateButtonText()
{
    QString text = m_mode == Mode::SpecificBuild
        ? tr("Update to Specific Build")
        : tr("Update System");
    ui->updateButton->setText(text);
}

void QnServerUpdatesWidget::updateButtonAccent()
{
    // 'Update' button accented by default if update is possible
    bool accented = m_lastUpdateCheckResult.result == QnCheckForUpdateResult::UpdateFound;

    // If warning is displayed, do not accent automatic update
    if (m_mode == Mode::LatestVersion && !ui->dayWarningBanner->isHidden())
        accented = false;

    if (accented)
        setAccentStyle(ui->updateButton);
    else
        resetButtonStyle(ui->updateButton);
}

void QnServerUpdatesWidget::updateDownloadButton()
{
    bool hasLatestVersion = m_mode == Mode::LatestVersion
        && !m_latestVersion.isNull()
        && m_updatesModel->lowestInstalledVersion() >= m_latestVersion;

    bool showButton = m_mode != QnServerUpdatesWidget::Mode::LocalFile
        && !hasLatestVersion;

    ui->downloadButton->setVisible(showButton);
    ui->downloadButton->setText(m_mode == Mode::LatestVersion
        ? tr("Download the Latest Version Update File")
        : tr("Download Update File"));
}

void QnServerUpdatesWidget::updateVersionPage()
{
    if (isUpdating())
        return;

    const bool hasLatestVersion = m_mode == Mode::LatestVersion
        && !m_latestVersion.isNull()
        && m_updatesModel->lowestInstalledVersion() >= m_latestVersion;

    ui->versionStackedWidget->setCurrentWidget(hasLatestVersion
        ? ui->latestVersionPage
        : ui->versionPage);

    if (hasLatestVersion)
        ui->infoStackedWidget->setCurrentWidget(ui->emptyInfoPage);
}

bool QnServerUpdatesWidget::cancelUpdate()
 {
    if (!m_updateTool->isUpdating())
        return true;

    ui->cancelButton->setEnabled(false);
    const bool result = m_updateTool->cancelUpdate();
    ui->cancelButton->setEnabled(true);
    return result;
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
    if (!m_updateTool->isUpdating())
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
            cancelUpdate();
    }
    else
    {
        QnMessageBox::warning(this,
            tr("Update cannot be canceled at this stage"),
            tr("Please wait until it is finished."));
    }
}

bool QnServerUpdatesWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return qnGlobalSettings->isUpdateNotificationsEnabled() != ui->updatesNotificationCheckbox->isChecked();
}

bool QnServerUpdatesWidget::canApplyChanges() const
{
    // TODO: #GDM now this prevents other tabs from saving their changes
    return !isUpdating();
}

bool QnServerUpdatesWidget::canDiscardChanges() const
{
    // TODO: #GDM now this prevents other tabs from discarding their changes
    return canCancelUpdate();
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
                ? tr("All components in your System are up to date.")
                : tr("All components in your System are up to this version.");
            break;

        case QnCheckForUpdateResult::InternetProblem:
            if (m_mode == Mode::LatestVersion)
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
            detail = tr("Incompatible %1 instance. To update disconnect System from %1 first.",
                "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
                .arg(nx::network::AppInfo::cloudName());
            break;

        default:
            NX_ASSERT(false); // should never get here
    }

    ui->errorLabel->setText(detail);
    ui->infoStackedWidget->setCurrentWidget(detail.isEmpty() ? ui->releaseNotesPage : ui->errorPage);

    m_releaseNotesUrl = result.releaseNotesUrl;
    ui->releaseNotesLabel->setVisible(!m_releaseNotesUrl.isEmpty());

    ui->releaseDescriptionLabel->setText(result.description);
    ui->releaseDescriptionLabel->setVisible(!result.description.isEmpty());

    ui->targetVersionLabel->setText(versionText);

    updateDownloadButton();
    updateButtonAccent();
    updateVersionPage();
}

bool QnServerUpdatesWidget::restartClient(const QnSoftwareVersion& version)
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

void QnServerUpdatesWidget::checkForUpdates(bool fromInternet)
{
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
                m_updatesModel->setLatestVersion(result.version);
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
    updateButtonAccent();

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
        resourcePool()->getResources<QnMediaServerResource>(lowFreeSpaceWarning.failedPeers);
    QnMessageBox dialog(QnMessageBoxIcon::Warning,
        tr("Not enough free space at %n Servers:", "", failedServers.size()),
        tr("Attempt to update may fail or cause Server malfunction."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        this);

    dialog.addCustomWidget(new QnResourceListView(failedServers, &dialog));
    dialog.addButton(tr("Force Update"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    const auto result = dialog.exec();
    if (result == QDialogButtonBox::Cancel)
        return;

    lowFreeSpaceWarning.ignore = true;
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

    updateVersionPage();

    if (isVisible())
    {
        switch (result.result)
        {
            case QnUpdateResult::Successful:
            {
                const bool clientUpdated = (result.targetVersion != qnStaticCommon->engineVersion());
                if (clientUpdated)
                {
                    if (result.clientInstallerRequired)
                    {
                        QnMessageBox::success(this,
                            tr("Server update completed"),
                            tr("Please update %1 manually using an installation package.")
                                .arg(QnClientAppInfo::applicationDisplayName()));
                    }
                    else
                    {
                        QnMessageBox::success(this,
                            tr("Update completed"),
                            tr("%1 will be restarted to the updated version.")
                                .arg(QnClientAppInfo::applicationDisplayName()));
                    }
                }
                else
                {
                    QnMessageBox::success(this, tr("Update completed"));
                }

                bool unholdConnection = !clientUpdated || result.clientInstallerRequired || result.protocolChanged;
                if (clientUpdated && !result.clientInstallerRequired)
                {
                    if (!restartClient(result.targetVersion))
                    {
                        unholdConnection = true;
                        QnConnectionDiagnosticsHelper::failedRestartClientMessage(this);
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
                    menu()->trigger(action::DisconnectAction, {Qn::ForceRole, true});

                break;
            }

            case QnUpdateResult::Cancelled:
                QnMessageBox::information(this, tr("Update canceled"));
                break;

            case QnUpdateResult::CancelledSilently:
                break;

            case QnUpdateResult::AlreadyUpdated:
                QnMessageBox::information(this, tr("All Servers already updated"));
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
                QnMessageBox::critical(this, tr("Update failed"), result.errorMessage());
                break;
        }
    }

    bool canUpdate = (result.result != QnUpdateResult::Successful) &&
        (result.result != QnUpdateResult::AlreadyUpdated);

    ui->updateButton->setEnabled(canUpdate);
}
