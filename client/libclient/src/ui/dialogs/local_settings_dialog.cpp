#include "local_settings_dialog.h"
#include "ui_local_settings_dialog.h"

#include <client/client_settings.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>

#include <ui/style/custom_style.h>

#include <ui/widgets/local_settings/general_preferences_widget.h>
#include <ui/widgets/local_settings/look_and_feel_preferences_widget.h>
#include <ui/widgets/local_settings/recording_settings_widget.h>
#include <ui/widgets/local_settings/popup_settings_widget.h>
#include <ui/widgets/local_settings/advanced_settings_widget.h>

#include <ui/workbench/workbench_context.h>

QnLocalSettingsDialog::QnLocalSettingsDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LocalSettingsDialog()),
    m_lookAndFeelWidget(new QnLookAndFeelPreferencesWidget(this)),
    m_advancedSettingsWidget(new QnAdvancedSettingsWidget(this)),
    m_restartLabel(nullptr)
{
    ui->setupUi(this);

    addPage(GeneralPage, new QnGeneralPreferencesWidget(this), tr("General"));
    addPage(LookAndFeelPage, m_lookAndFeelWidget, tr("Look and Feel"));

    auto recordingSettingsWidget = new QnRecordingSettingsWidget(this);

    const auto screenRecordingAction = action(QnActions::ToggleScreenRecordingAction);
    addPage(
        RecordingPage,
        recordingSettingsWidget,
        screenRecordingAction ? tr("Screen Recording") : tr("Audio Settings") );


    addPage(NotificationsPage, new QnPopupSettingsWidget(this), tr("Notifications"));
    addPage(AdvancedPage, m_advancedSettingsWidget, tr("Advanced"));

    setWarningStyle(ui->readOnlyWarningLabel);
    ui->readOnlyWarningLabel->setVisible(!qnSettings->isWritable());
    ui->readOnlyWarningLabel->setText(
#ifdef Q_OS_LINUX
        tr("Settings file is read-only. Please contact your system administrator. All changes will be lost after program exit.")
#else
        tr("Settings cannot be saved. Please contact your system administrator. All changes will be lost after program exit.")
#endif
    );
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
    if (qnSettings->isWritable())
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
        QDialogButtonBox::StandardButton result = QnMessageBox::information(
            this,
            tr("Information"),
            tr("Some changes will take effect only after application restart. Do you want to restart the application now?"),
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            QDialogButtonBox::Yes);
        switch (result)
        {
            case QDialogButtonBox::Cancel:
                return;
            case QDialogButtonBox::Yes:
                restartQueued = true;
                break;
            default:
                break;
        }
    }

    base_type::accept();

    if (restartQueued)
        menu()->trigger(QnActions::QueueAppRestartAction);
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

