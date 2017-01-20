#include "advanced_settings_widget.h"
#include "ui_advanced_settings_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>

#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/resource_directory_browser.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_auto_starter.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/local_file_cache.h>

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::AdvancedSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->browseLogsButton,          Qn::SystemSettings_General_Logs_Help);
    setHelpTopic(ui->doubleBufferCheckbox,      Qn::SystemSettings_General_DoubleBuffering_Help);
    setHelpTopic(ui->doubleBufferWarningLabel,  Qn::SystemSettings_General_DoubleBuffering_Help);

    setWarningStyle(ui->doubleBufferWarningLabel);
    ui->doubleBufferWarningLabel->setVisible(false);

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
            ui->doubleBufferWarningLabel->setVisible(!toggled && qnSettings->isGlDoubleBuffer());
            emit hasChangesChanged();
        });

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
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{
}

void QnAdvancedSettingsWidget::applyChanges()
{
    qnSettings->setAudioDownmixed(isAudioDownmixed());
    qnSettings->setGLDoubleBuffer(isDoubleBufferingEnabled());
    qnSettings->setMaximumLiveBufferMSecs(maximumLiveBufferMs());
}

void QnAdvancedSettingsWidget::loadDataToUi()
{
    setAudioDownmixed(qnSettings->isAudioDownmixed());
    setDoubleBufferingEnabled(qnSettings->isGlDoubleBuffer());
    setMaximumLiveBufferMs(qnSettings->maximumLiveBufferMSecs());
}

bool QnAdvancedSettingsWidget::hasChanges() const
{
    return qnSettings->isAudioDownmixed() != isAudioDownmixed()
        || qnSettings->isGlDoubleBuffer() != isDoubleBufferingEnabled()
        || qnSettings->maximumLiveBufferMSecs() != maximumLiveBufferMs();
}

bool QnAdvancedSettingsWidget::isRestartRequired() const
{
    /* These changes can be applied only after client restart. */
    return qnRuntime->isGlDoubleBuffer() != isDoubleBufferingEnabled();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnAdvancedSettingsWidget::at_browseLogsButton_clicked()
{
    const QString logsLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + lit("/log");
    if (!QDir(logsLocation).exists())
    {
        QnMessageBox::_warning(this, tr("Folder not found"), logsLocation);
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
        QnLocalFileCache cache;
        QString path = cache.getFullPath(backgroundImage);
        QFile lock(path);
        lock.open(QFile::ReadWrite);
        QnAppServerFileCache::clearLocalCache();
        lock.close();
    }
    else
    {
        QnAppServerFileCache::clearLocalCache();
    }
}

void QnAdvancedSettingsWidget::at_resetAllWarningsButton_clicked()
{
    qnSettings->setShowOnceMessages(0);
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

int QnAdvancedSettingsWidget::maximumLiveBufferMs() const
{
    return ui->maximumLiveBufferLengthSlider->value();
}

void QnAdvancedSettingsWidget::setMaximumLiveBufferMs(int value)
{
    ui->maximumLiveBufferLengthSlider->setValue(value);
}
