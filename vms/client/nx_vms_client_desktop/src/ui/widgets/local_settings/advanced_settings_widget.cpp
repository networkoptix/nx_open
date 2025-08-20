// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_settings_widget.h"
#include "ui_advanced_settings_widget.h"

#include <thread>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>

#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <core/resource/resource_directory_browser.h>
#include <nx/build_info.h>
#include <nx/media/hardware_acceleration_type.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/text/time_strings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

using namespace nx::vms::client::desktop;

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight16Theme = {
    {QIcon::Normal, {.primary = "light4"}},
    {QIcon::Active, {.primary = "light4"}},
    {QIcon::Selected, {.primary = "light4"}},
};

NX_DECLARE_COLORIZED_ICON(kDownloadIcon, "20x20/Outline/download.svg", kLight16Theme)
NX_DECLARE_COLORIZED_ICON(kFolderIcon, "20x20/Outline/folder.svg", kLight16Theme)
NX_DECLARE_COLORIZED_ICON(kSettingsIcon, "20x20/Outline/settings.svg", kLight16Theme)


} // namespace

struct QnAdvancedSettingsWidget::Private
{
    std::unique_ptr<ClientLogCollector> logsCollector;
    QString path;
    bool finished = false;
    bool success = false;
};

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AdvancedSettingsWidget),
    d(new Private())
{
    ui->setupUi(this);

    ui->dialogContentsLayout->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);
    ui->dialogContentsLayout->setSpacing(nx::style::Metrics::kDefaultLayoutSpacing.height());

    if (!nx::build_info::isMacOsX())
    {
        ui->autoFpsLimitCheckbox->setVisible(false);
        ui->autoFpsLimitCheckboxHint->setVisible(false);
    }

    if (nx::media::getHardwareAccelerationType() == nx::media::HardwareAccelerationType::none)
        ui->useHardwareDecodingCheckbox->setEnabled(false);

    if (appContext()->runtimeSettings()->graphicsApi() == GraphicsApi::software)
        ui->disableBlurCheckbox->setEnabled(false);

    // Legacy Nvidia NVDEC and Intel Quick Sync decoders have their own settings.
    if (appContext()->runtimeSettings()->graphicsApi() == GraphicsApi::legacyopengl)
        ui->maxHardwareDecodingStreamsWidget->setVisible(false);

    ui->maximumLiveBufferLengthSpinBox->setSuffix(' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Milliseconds));

    setHelpTopic(ui->downmixAudioCheckBox, HelpTopic::Id::MainWindow_MediaItem_Local);
    setHelpTopic(ui->disableBlurCheckbox, HelpTopic::Id::LocalSettings_Advanced_Blur);
    setHelpTopic(ui->doubleBufferingWidget, HelpTopic::Id::SystemSettings_General_DoubleBuffering);
    setHelpTopic(ui->useHardwareDecodingLayout, HelpTopic::Id::LocalSettings_Advanced_HardwareDecode);

    setHelpTopic(
        {ui->maximumLiveBufferLengthLabelLayout, ui->liveBufferEditLayout},
        HelpTopic::Id::LocalSettings_Advanced_LiveBuffer);

    setHelpTopic(ui->certificateLayout, HelpTopic::Id::CertificateValidation);

    setHelpTopic(ui->clearCacheButton, HelpTopic::Id::MainWindow_WebPageItem);

    setHelpTopic(ui->logsManagementBox, HelpTopic::Id::SystemSettings_General_Logs);

    ui->doubleBufferCheckboxHint->setHintText(
        tr("Helps avoid problems with OpenGL drawing which result in 100% CPU load."));

    ui->autoFpsLimitCheckboxHint->setHintText(
        tr("Warning! This is an experimental option that saves CPU but may affect animation."));

    ui->maximumLiveBufferLengthLabelHint->setHintText(
        tr("Adjust to smallest value that does not degrade live view. Bigger buffer makes playback "
            "smoother but increases delay between real time and live view; smaller buffer "
            "decreases delay but can cause stutters."));

    const auto combobox = ui->certificateValidationLevelComboBox;
    combobox->addItem(tr("Disabled"),
        QVariant::fromValue(ServerCertificateValidationLevel::disabled));
    combobox->addItem(tr("Recommended"),
        QVariant::fromValue(ServerCertificateValidationLevel::recommended));
    combobox->addItem(tr("Strict"),
        QVariant::fromValue(ServerCertificateValidationLevel::strict));

    ui->downloadLogsButton->setIcon(qnSkin->icon(kDownloadIcon));
    ui->logsSettingsButton->setIcon(qnSkin->icon(kSettingsIcon));
    ui->openLogsFolderButton->setIcon(qnSkin->icon(kFolderIcon));
    ui->openLogsFolderButton->setFlat(true);

    ui->logsManagementBox->setVisible(appContext()->localSettings()->browseLogsButtonVisible());

    connect(ui->clearCacheButton, &QPushButton::clicked, this,
        &QnAdvancedSettingsWidget::at_clearCacheButton_clicked);
    connect(ui->resetAllWarningsButton, &QPushButton::clicked, this,
        &QnAdvancedSettingsWidget::at_resetAllWarningsButton_clicked);

    connect(ui->downmixAudioCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->doubleBufferCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->autoFpsLimitCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->disableBlurCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->useHardwareDecodingCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->maxHardwareDecodingStreamsSpinBox, QnSpinboxIntValueChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    // Live buffer lengths slider/spin logic.
    connect(ui->maximumLiveBufferLengthSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            ui->maximumLiveBufferLengthSpinBox->setValue(value);
            emit hasChangesChanged();
        });

    connect(ui->maximumLiveBufferLengthSpinBox, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            ui->maximumLiveBufferLengthSlider->setValue(value);
            emit hasChangesChanged();
        });

    connect(combobox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(combobox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &QnAdvancedSettingsWidget::updateCertificateValidationLevelDescription);

    connect(ui->downloadLogsButton, &QPushButton::clicked, this,
        [this]
        {
            d->finished = d->success = false;
            QString dir = QFileDialog::getExistingDirectory(
                this,
                tr("Select Folder..."),
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
            if (dir.isEmpty())
                return;

            d->path = dir;

            const auto time = QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");
            d->logsCollector.reset(new ClientLogCollector(
                dir + QString("/client_%1.zip").arg(time)));

            connect(d->logsCollector.get(), &ClientLogCollector::success, this,
                [this]
                {
                    d->finished = d->success = true;
                    updateLogsManagementWidgetsState();
                });
            connect(d->logsCollector.get(), &ClientLogCollector::error, this,
                [this]
                {
                    d->finished = true;
                    d->success = false;
                    updateLogsManagementWidgetsState();
                });
            connect(d->logsCollector.get(), &ClientLogCollector::progressChanged, this,
                [this](double value)
                {
                    ui->logsDownloadProgressBar->setValue(value);
                });
            d->logsCollector->start();

            updateLogsManagementWidgetsState();
        });
    connect(ui->logsSettingsButton, &QPushButton::clicked, this,
        [this]
        {
            auto watcher = context()->system()->logsManagementWatcher();
            if (!NX_ASSERT(watcher))
                return;

            auto dialog = createSelfDestructingDialog<LogSettingsDialog>(mainWindowWidget());
            dialog->init({watcher->clientUnit()});

            connect(
                dialog,
                &LogSettingsDialog::accepted,
                this,
                [dialog, watcher]
                {
                    if (!dialog->hasChanges())
                        return;

                    watcher->storeClientSettings(dialog->changes());
                });

            dialog->show();
        });
    connect(ui->openLogsFolderButton, &QPushButton::clicked, this,
        [this]
        {
            if (!d->path.isEmpty())
                QDesktopServices::openUrl(QUrl::fromLocalFile(d->path));
        });
    auto resetLogsDownloadState =
        [this]
        {
            d->logsCollector.reset();
            d->path.clear();
            d->finished = d->success = false;

            updateLogsManagementWidgetsState();
        };
    connect(ui->cancelLogsDownloadButton, &QPushButton::clicked, this, resetLogsDownloadState);
    connect(ui->logsDownloadDoneButton, &QPushButton::clicked, this, resetLogsDownloadState);
    connect(this, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnAdvancedSettingsWidget::updateBannerVisibility);

    ui->doubleBufferingWidget->setVisible(nx::build_info::isWindows() || ini().developerMode);
    setPaletteColor(
        ui->horizontalLine, QPalette::Shadow, nx::vms::client::core::colorTheme()->color("dark12"));
    ui->alertLabel->setType(AlertLabel::Type::warning);
    ui->alertLabel->setText(
        tr("Note: Image Enhancement is not available when Hardware Video Decoding is turned on."));
    updateBannerVisibility();
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{
}

void QnAdvancedSettingsWidget::applyChanges()
{
    appContext()->localSettings()->downmixAudio = isAudioDownmixed();
    appContext()->localSettings()->glDoubleBuffer = isDoubleBufferingEnabled();
    appContext()->localSettings()->autoFpsLimit = isAutoFpsLimitEnabled();
    appContext()->localSettings()->maximumLiveBufferMs = maximumLiveBufferMs();
    appContext()->localSettings()->glBlurEnabled = isBlurEnabled();
    appContext()->localSettings()->hardwareDecodingEnabledProperty = isHardwareDecodingEnabled();
    appContext()->localSettings()->maxHardwareDecodingStreams = maxHardwareDecodingStreams();

    const auto oldCertificateValidationLevel =
        appContext()->coreSettings()->certificateValidationLevel();
    const auto newCertificateValidationLevel = certificateValidationLevel();

    if (oldCertificateValidationLevel != newCertificateValidationLevel)
    {
        const auto checkConnection =
            [this]
            {
                return (bool)connection();
            };
        const auto checkInstances =
            []
            {
                return appContext()->sharedMemoryManager()->runningInstancesIndices().size() > 1;
            };

        const bool connected = checkConnection();
        const bool otherWindowsExist = checkInstances();

        const auto updateSecurityLevel =
            [level=newCertificateValidationLevel]()
            {
                appContext()->coreSettings()->certificateValidationLevel.setValue(level);
                // Certificate Verifier and related classes are updated by QnClientCoreModule.
            };

        if (connected || otherWindowsExist)
        {
            // Prepare warning message.
            QStringList message;
            message << tr("Certificate storage will be cleared.");
            if (connected)
                message << tr("Current client instance will be disconnected.");
            if (otherWindowsExist)
                message << tr("All other client windows will be closed.");

            QnMessageBox box(
                QnMessageBox::Icon::Warning,
                tr("Security settings changed"),
                message.join("\n"),
                QDialogButtonBox::Cancel,
                QDialogButtonBox::NoButton,
                this);

            // Configure action button.
            auto okButton = new BusyIndicatorButton(&box);
            okButton->setText(tr("Continue"));
            box.addButton(okButton, QDialogButtonBox::ActionRole);
            box.setDefaultButton(okButton);

            connect(okButton, &QPushButton::clicked, &box,
                [=, box=&box]
                {
                    if (okButton->isIndicatorVisible())
                        return; //< The button has been clicked already.

                    // Update button.
                    okButton->showIndicator();

                    // Stop other clients.
                    appContext()->sharedMemoryManager()->requestToExit();

                    // Disconnect.
                    menu()->trigger(menu::DisconnectAction);

                    // Wait until all clients are closed or disconnected.
                    auto timer = new QTimer(box);
                    connect(timer, &QTimer::timeout, box,
                        [=]
                        {
                            // Wait.
                            if (checkConnection() || checkInstances())
                                return;

                            // Apply changes and close the dialog.
                            timer->stop();
                            updateSecurityLevel();
                            box->accept();
                        });
                    timer->start(100);
                });

            // Show warning dialog.
            box.exec();
        }
        else
        {
            // Just update settings.
            updateSecurityLevel();
        }
    }
    updateBannerVisibility();
}

void QnAdvancedSettingsWidget::loadDataToUi()
{
    setAudioDownmixed(appContext()->localSettings()->downmixAudio());
    setDoubleBufferingEnabled(appContext()->localSettings()->glDoubleBuffer());
    setAutoFpsLimitEnabled(appContext()->localSettings()->autoFpsLimit());
    setMaximumLiveBufferMs(appContext()->localSettings()->maximumLiveBufferMs());
    setBlurEnabled(appContext()->localSettings()->glBlurEnabled());
    setHardwareDecodingEnabled(appContext()->localSettings()->hardwareDecodingEnabled());
    setMaxHardwareDecodingStreams(appContext()->localSettings()->maxHardwareDecodingStreams());
    setCertificateValidationLevel(appContext()->coreSettings()->certificateValidationLevel());
    updateCertificateValidationLevelDescription();
}

bool QnAdvancedSettingsWidget::hasChanges() const
{
    return appContext()->localSettings()->downmixAudio() != isAudioDownmixed()
        || appContext()->localSettings()->glDoubleBuffer() != isDoubleBufferingEnabled()
        || appContext()->localSettings()->autoFpsLimit() != isAutoFpsLimitEnabled()
        || appContext()->localSettings()->maximumLiveBufferMs() != maximumLiveBufferMs()
        || appContext()->localSettings()->glBlurEnabled() != isBlurEnabled()
        || appContext()->localSettings()->hardwareDecodingEnabled() != isHardwareDecodingEnabled()
        || appContext()->localSettings()->maxHardwareDecodingStreams() != maxHardwareDecodingStreams()
        || appContext()->coreSettings()->certificateValidationLevel()
            != certificateValidationLevel();
}

bool QnAdvancedSettingsWidget::isRestartRequired() const
{
    /* These changes can be applied only after client restart. */
    return (qnRuntime->isGlDoubleBuffer() != isDoubleBufferingEnabled());
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnAdvancedSettingsWidget::at_clearCacheButton_clicked()
{
    QString backgroundImage = appContext()->localSettings()->backgroundImage().imagePath();
    /* Lock background image so it will not be deleted. */
    if (!backgroundImage.isEmpty())
    {
        QString path = nx::vms::client::desktop::LocalFileCache::fullPath(
            backgroundImage,
            Qn::kWallpapersFolder);
        QFile lock(path);
        lock.open(QFile::ReadWrite);
        nx::vms::client::desktop::ServerFileCache::clearLocalCache();
        lock.close();
    }
    else
    {
        nx::vms::client::desktop::ServerFileCache::clearLocalCache();
    }
    // Remove all Qt WebEngine profile directories.
    const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    static constexpr auto kWebEngineDirName = "QtWebEngine";
    auto webEngineData = QDir(QDir(dataLocation).filePath(kWebEngineDirName));
    if (webEngineData.exists())
    {
        std::thread([](QDir dir)
            {
                if (dir.removeRecursively())
                    return;
                NX_ERROR(typeid(QnAdvancedSettingsWidget), "Unable to fully remove %1", dir);
            }, webEngineData).detach();
    }
}

void QnAdvancedSettingsWidget::at_resetAllWarningsButton_clicked()
{
    showOnceSettings()->reset();
    messageBarSettings()->reset();
}

bool QnAdvancedSettingsWidget::isAudioDownmixed() const
{
    return ui->downmixAudioCheckBox->isChecked();
}

void QnAdvancedSettingsWidget::setAudioDownmixed(bool value)
{
    ui->downmixAudioCheckBox->setChecked(value);
}

bool QnAdvancedSettingsWidget::isDoubleBufferingEnabled() const
{
    return ui->doubleBufferCheckbox->isChecked();
}

void QnAdvancedSettingsWidget::setDoubleBufferingEnabled(bool value)
{
    ui->doubleBufferCheckbox->setChecked(value);
}

bool QnAdvancedSettingsWidget::isAutoFpsLimitEnabled() const
{
    return ui->autoFpsLimitCheckbox->isChecked();
}

void QnAdvancedSettingsWidget::setAutoFpsLimitEnabled(bool value)
{
    ui->autoFpsLimitCheckbox->setChecked(value);
}

bool QnAdvancedSettingsWidget::isBlurEnabled() const
{
    return !ui->disableBlurCheckbox->isChecked();
}

void QnAdvancedSettingsWidget::setBlurEnabled(bool value)
{
    if (!ui->disableBlurCheckbox->isEnabled())
    {
        ui->disableBlurCheckbox->setChecked(true);
        return;
    }

    ui->disableBlurCheckbox->setChecked(!value);
}

bool QnAdvancedSettingsWidget::isHardwareDecodingEnabled() const
{
    return ui->useHardwareDecodingCheckbox->isChecked();
}

void QnAdvancedSettingsWidget::setHardwareDecodingEnabled(bool value)
{
    // TODO: #vbreus If nx::media::getHardwareAccelerationType returns 'none' then 'Use hardware
    // video decoding check box should be unchecked and disabled.
    // Also, in this case localSettings()->hardwareDecodingEnabled() should return false
    // disregarding previously set value, and there is no proper way to implement such behavior for
    // now.
    ui->useHardwareDecodingCheckbox->setChecked(value);
}

int QnAdvancedSettingsWidget::maxHardwareDecodingStreams() const
{
    return ui->maxHardwareDecodingStreamsSpinBox->value();
}

void QnAdvancedSettingsWidget::setMaxHardwareDecodingStreams(int value)
{
    ui->maxHardwareDecodingStreamsSpinBox->setValue(value);
}

int QnAdvancedSettingsWidget::maximumLiveBufferMs() const
{
    return ui->maximumLiveBufferLengthSlider->value();
}

void QnAdvancedSettingsWidget::setMaximumLiveBufferMs(int value)
{
    ui->maximumLiveBufferLengthSlider->setValue(value);
}

QnAdvancedSettingsWidget::ServerCertificateValidationLevel
    QnAdvancedSettingsWidget::certificateValidationLevel() const
{
    const auto combobox = ui->certificateValidationLevelComboBox;
    return combobox->currentData().value<ServerCertificateValidationLevel>();
}

void QnAdvancedSettingsWidget::setCertificateValidationLevel(ServerCertificateValidationLevel value)
{
    const auto combobox = ui->certificateValidationLevelComboBox;
    const auto idx = combobox->findData(QVariant::fromValue(value));
    NX_ASSERT(idx != -1);
    combobox->setCurrentIndex(idx);
}

void QnAdvancedSettingsWidget::updateCertificateValidationLevelDescription()
{
    const auto level = certificateValidationLevel();
    QString text;
    bool isWarning = false;
    switch (level)
    {
        case ServerCertificateValidationLevel::disabled:
            text = tr("May lead to privacy issues");
            isWarning = true;
            break;
        case ServerCertificateValidationLevel::recommended:
            text = tr("On the first connection to the server, your confirmation will be requested"
                " to accept the certificate if it contains errors");
            break;
        case ServerCertificateValidationLevel::strict:
            text = tr("Connect only servers with public certificate");
            break;
    }

    auto label = ui->certificateValidationDescriptionLabel;
    label->setText(text);

    setPaletteColor(
        label,
        QPalette::WindowText,
        nx::vms::client::core::colorTheme()->color(isWarning ? "red" : "dark16"));
}

void QnAdvancedSettingsWidget::updateLogsManagementWidgetsState()
{
    bool downloadStarted = false;
    bool downloadFinished = false;
    bool hasErrors = false;

    if (d->logsCollector)
    {
        downloadStarted = true;
        downloadFinished = d->finished;
        hasErrors = !d->success;
    }

    ui->logsManagementStack->setCurrentWidget(
        downloadStarted ? ui->logsDownloadPage : ui->logsDefaultPage);

    ui->logsDownloadProgressBar->setVisible(!downloadFinished);
    ui->cancelLogsDownloadButton->setVisible(downloadStarted && !downloadFinished);
    ui->logsDownloadDoneButton->setVisible(downloadFinished);

    ui->logsDownloadStatusLabel->setVisible(downloadFinished);
    ui->logsDownloadStatusLabel->setText(hasErrors
        ? tr("Failed to save logs to the selected folder")
        : tr("Download complete!"));
    setWarningStyleOn(ui->logsDownloadStatusLabel, hasErrors);
}

void QnAdvancedSettingsWidget::updateBannerVisibility()
{
    bool visible = isHardwareDecodingEnabled()
        && not appContext()->localSettings()->hardwareDecodingEnabled();
    ui->horizontalLine->setVisible(visible);
    ui->alertLabel->setVisible(visible);
}
