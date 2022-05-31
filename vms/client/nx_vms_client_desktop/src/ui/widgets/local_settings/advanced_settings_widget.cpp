// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_settings_widget.h"
#include "ui_advanced_settings_widget.h"

#include <thread>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>

#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/client_show_once_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/resource_directory_browser.h>
#include <nx/build_info.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/text/time_strings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

using namespace nx::vms::client::desktop;

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AdvancedSettingsWidget)
{
    ui->setupUi(this);

    if (!nx::build_info::isMacOsX())
    {
        ui->autoFpsLimitCheckbox->setVisible(false);
        ui->autoFpsLimitCheckboxHint->setVisible(false);
    }

    ui->maximumLiveBufferLengthSpinBox->setSuffix(' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Milliseconds));

    setHelpTopic(ui->browseLogsButton, Qn::SystemSettings_General_Logs_Help);
    setHelpTopic(ui->doubleBufferCheckbox, Qn::SystemSettings_General_DoubleBuffering_Help);

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

    ui->browseLogsButton->setVisible(qnSettings->isBrowseLogsVisible());

    connect(ui->browseLogsButton, &QPushButton::clicked, this,
        &QnAdvancedSettingsWidget::at_browseLogsButton_clicked);
    connect(ui->clearCacheButton, &QPushButton::clicked, this,
        &QnAdvancedSettingsWidget::at_clearCacheButton_clicked);
    connect(ui->resetAllWarningsButton, &QPushButton::clicked, this,
        &QnAdvancedSettingsWidget::at_resetAllWarningsButton_clicked);

    connect(ui->downmixAudioCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->doubleBufferCheckbox, &QCheckBox::toggled, this,
        [this](bool toggled)
        {
            /* Show warning message if the user disables double buffering. */
            // TODO: Being replaced by HintButton
            //ui->doubleBufferWarningLabel->setVisible(!toggled && qnSettings->isGlDoubleBuffer());
            emit hasChangesChanged();
        });

    connect(ui->autoFpsLimitCheckbox, &QCheckBox::toggled, this,
        [this](bool toggled)
        {
            emit hasChangesChanged();
        });

    connect(ui->disableBlurCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->useHardwareDecodingCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    /* Live buffer lengths slider/spin logic: */
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

}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{
}

void QnAdvancedSettingsWidget::applyChanges()
{
    qnSettings->setAudioDownmixed(isAudioDownmixed());
    qnSettings->setGLDoubleBuffer(isDoubleBufferingEnabled());
    qnSettings->setAutoFpsLimit(isAutoFpsLimitEnabled());
    qnSettings->setMaximumLiveBufferMs(maximumLiveBufferMs());
    qnSettings->setGlBlurEnabled(isBlurEnabled());
    qnSettings->setHardwareDecodingEnabled(isHardwareDecodingEnabled());

    const auto oldCertificateValidationLevel =
        nx::vms::client::core::settings()->certificateValidationLevel();
    const auto newCertificateValidationLevel = certificateValidationLevel();

    if (oldCertificateValidationLevel != newCertificateValidationLevel)
    {
        const auto checkConnection =
            [this]
            {
                return (bool)connection();
            };
        const auto checkInstances =
            [this]
            {
                return appContext()->sharedMemoryManager()->runningInstancesIndices().size() > 1;
            };

        const bool connected = checkConnection();
        const bool otherWindowsExist = checkInstances();

        const auto updateSecurityLevel =
            [this, level=newCertificateValidationLevel]()
            {
                qnClientCoreModule->networkModule()->reinitializeCertificateStorage(level);
                nx::vms::client::core::settings()->certificateValidationLevel.setValue(level);
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
                    menu()->trigger(ui::action::DisconnectAction);

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
}

void QnAdvancedSettingsWidget::loadDataToUi()
{
    setAudioDownmixed(qnSettings->isAudioDownmixed());
    setDoubleBufferingEnabled(qnSettings->isGlDoubleBuffer());
    setAutoFpsLimitEnabled(qnSettings->isAutoFpsLimit());
    setMaximumLiveBufferMs(qnSettings->maximumLiveBufferMs());
    setBlurEnabled(qnSettings->isGlBlurEnabled());
    setHardwareDecodingEnabled(qnSettings->isHardwareDecodingEnabled());
    setCertificateValidationLevel(nx::vms::client::core::settings()->certificateValidationLevel());
    updateCertificateValidationLevelDescription();
}

bool QnAdvancedSettingsWidget::hasChanges() const
{
    return qnSettings->isAudioDownmixed() != isAudioDownmixed()
        || qnSettings->isGlDoubleBuffer() != isDoubleBufferingEnabled()
        || qnSettings->isAutoFpsLimit() != isAutoFpsLimitEnabled()
        || qnSettings->maximumLiveBufferMs() != maximumLiveBufferMs()
        || qnSettings->isGlBlurEnabled() != isBlurEnabled()
        || qnSettings->isHardwareDecodingEnabled() != isHardwareDecodingEnabled()
        || nx::vms::client::core::settings()->certificateValidationLevel() != certificateValidationLevel();
}

bool QnAdvancedSettingsWidget::isRestartRequired() const
{
    /* These changes can be applied only after client restart. */
    return (qnRuntime->isGlDoubleBuffer() != isDoubleBufferingEnabled())
        || (qnRuntime->maximumLiveBufferMs() != maximumLiveBufferMs());
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnAdvancedSettingsWidget::at_browseLogsButton_clicked()
{
    const QString logsLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + lit("/log");
    if (!QDir(logsLocation).exists())
    {
        QnMessageBox::warning(this, tr("Folder not found"), logsLocation);
        return;
    }
    QDesktopServices::openUrl(QLatin1String("file:///") + logsLocation);
}

void QnAdvancedSettingsWidget::at_clearCacheButton_clicked()
{
    QString backgroundImage = qnSettings->backgroundImage().name;
    /* Lock background image so it will not be deleted. */
    if (!backgroundImage.isEmpty())
    {
        nx::vms::client::desktop::LocalFileCache cache;
        QString path = cache.getFullPath(backgroundImage);
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
    qnClientShowOnce->reset();
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
    ui->disableBlurCheckbox->setChecked(!value);
}

bool QnAdvancedSettingsWidget::isHardwareDecodingEnabled() const
{
    return ui->useHardwareDecodingCheckbox->isChecked();
}

void QnAdvancedSettingsWidget::setHardwareDecodingEnabled(bool value)
{
    ui->useHardwareDecodingCheckbox->setChecked(value);
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
        nx::vms::client::desktop::colorTheme()->color(isWarning ? "red_l2" : "dark14"));
}
