#include "local_settings_dialog.h"
#include "ui_local_settings_dialog.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <client/client_settings.h>
#include <client/client_app_info.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

#include <ui/screen_recording/video_recorder_settings.h>
#include <ui/style/custom_style.h>

#include <ui/widgets/local_settings/general_preferences_widget.h>
#include <ui/widgets/local_settings/look_and_feel_preferences_widget.h>
#include <ui/widgets/local_settings/recording_settings_widget.h>
#include <ui/widgets/local_settings/popup_settings_widget.h>
#include <ui/widgets/local_settings/advanced_settings_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_desktop_camera_watcher.h>

using namespace nx::client::desktop::ui;

QnLocalSettingsDialog::QnLocalSettingsDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LocalSettingsDialog()),
    m_lookAndFeelWidget(new QnLookAndFeelPreferencesWidget(this)),
    m_advancedSettingsWidget(new QnAdvancedSettingsWidget(this)),
    m_restartLabel(nullptr)
{
    ui->setupUi(this);
    auto recorderSettings = new QnVideoRecorderSettings(this);
    auto updateRecorderSettings = [this]
        {
            context()->instance<QnWorkbenchDesktopCameraWatcher>()->forcedUpdate();
        };


    auto generalPageWidget = new QnGeneralPreferencesWidget(recorderSettings, this);
    addPage(GeneralPage, generalPageWidget, tr("General"));
    connect(generalPageWidget, &QnGeneralPreferencesWidget::recordingSettingsChanged, this,
        updateRecorderSettings);

    connect(generalPageWidget, &QnGeneralPreferencesWidget::mediaDirectoriesChanged, this,
        [this]
        {
            context()->menu()->trigger(action::UpdateLocalFilesAction);
        });

    addPage(LookAndFeelPage, m_lookAndFeelWidget, tr("Look and Feel"));

    const auto screenRecordingAction = action(action::ToggleScreenRecordingAction);
    if (screenRecordingAction)
    {
        auto recordingSettingsWidget = new QnRecordingSettingsWidget(recorderSettings, this);
        addPage(RecordingPage, recordingSettingsWidget, tr("Screen Recording"));
        connect(recordingSettingsWidget, &QnRecordingSettingsWidget::recordingSettingsChanged, this,
            updateRecorderSettings);
    }

    addPage(NotificationsPage, new QnPopupSettingsWidget(this), tr("Notifications"));
    addPage(AdvancedPage, m_advancedSettingsWidget, tr("Advanced"));

    setWarningStyle(ui->readOnlyWarningLabel);
    ui->readOnlyWarningWidget->setVisible(!qnSettings->isWritable());
    ui->readOnlyWarningLabel->setText(
#ifdef Q_OS_LINUX
        tr("Settings file is read-only. Please contact your system administrator. All changes will be lost after program exit.")
#else
        tr("Settings cannot be saved. Please contact your system administrator. All changes will be lost after program exit.")
#endif
    );

    //;

    addRestartLabel();

    loadDataToUi();
}

QnLocalSettingsDialog::~QnLocalSettingsDialog() {}

bool QnLocalSettingsDialog::canApplyChanges() const
{
    return qnSettings->isWritable()
        && base_type::canApplyChanges();
}

void QnLocalSettingsDialog::applyChanges()
{
    base_type::applyChanges();
    qnSettings->save();
}

void QnLocalSettingsDialog::updateButtonBox()
{
    base_type::updateButtonBox();
    m_restartLabel->setVisible(isRestartRequired());
}

void QnLocalSettingsDialog::accept()
{
    bool restartQueued = false;

    if (isRestartRequired())
    {
        QnMessageBox dialog(this);
        dialog.setText(tr("Some changes will take effect only after %1 restart")
            .arg(QnClientAppInfo::applicationDisplayName()));

        dialog.setStandardButtons(QDialogButtonBox::Cancel);
        const auto restartNowButton = dialog.addButton(
            tr("Restart Now"), QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);

        dialog.addButton(
            tr("Restart Later"), QDialogButtonBox::NoRole);

        if (dialog.exec() == QDialogButtonBox::Cancel)
            return;

        if (dialog.clickedButton() == restartNowButton)
            restartQueued = true;
    }

    base_type::accept();

    if (restartQueued)
        menu()->trigger(action::QueueAppRestartAction);
}

bool QnLocalSettingsDialog::isRestartRequired() const
{
    return m_advancedSettingsWidget->isRestartRequired()
        || m_lookAndFeelWidget->isRestartRequired();
}

void QnLocalSettingsDialog::addRestartLabel()
{
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(ui->buttonBox->layout());
    NX_ASSERT(layout, Q_FUNC_INFO, "Layout must already exist here.");
    if (!layout)
        return;

    m_restartLabel = new QLabel(tr("Restart required"), ui->buttonBox);
    setWarningStyle(m_restartLabel);
    m_restartLabel->setVisible(false);

    layout->insertWidget(0, m_restartLabel);
}

