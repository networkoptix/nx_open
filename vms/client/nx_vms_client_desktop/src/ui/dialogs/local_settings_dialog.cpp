// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_settings_dialog.h"
#include "ui_local_settings_dialog.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <nx/branding.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/screen_recording_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/local_settings/advanced_settings_widget.h>
#include <ui/widgets/local_settings/general_preferences_widget.h>
#include <ui/widgets/local_settings/look_and_feel_preferences_widget.h>
#include <ui/widgets/local_settings/popup_settings_widget.h>
#include <ui/widgets/local_settings/recording_settings_widget.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

namespace {

QDialogButtonBox::StandardButton getRestartDialogAnswer()
{
    QnMessageBox dialog(nullptr);
    dialog.setText(QnLocalSettingsDialog::tr("Some changes will take effect only after %1 restart")
        .arg(nx::branding::desktopClientDisplayName()));
    dialog.setIcon(QnMessageBox::Icon::Question);

    dialog.setStandardButtons(QDialogButtonBox::Cancel);
    const auto restartNowButton = dialog.addButton(
        QnLocalSettingsDialog::tr("Restart Now"),
        QDialogButtonBox::YesRole,
        Qn::ButtonAccent::Standard);

    dialog.addButton(
        QnLocalSettingsDialog::tr("Restart Later"), QDialogButtonBox::NoRole);

    auto result = static_cast<QDialogButtonBox::StandardButton>(dialog.exec());
    if ((result == QDialogButtonBox::NoButton) && (dialog.clickedButton() == restartNowButton))
        result = QDialogButtonBox::Yes;

    return result;
}

} // namespace

QnLocalSettingsDialog::QnLocalSettingsDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LocalSettingsDialog()),
    m_lookAndFeelWidget(new QnLookAndFeelPreferencesWidget(this)),
    m_advancedSettingsWidget(new QnAdvancedSettingsWidget(this)),
    m_restartLabel(nullptr)
{
    ui->setupUi(this);
    auto updateRecorderSettings =
        [this]
        {
            // Desktop camera will be reinitialized here. Actually this code must be removed as
            // soon as 2-way audio is reimplemented, so method will be called only on "Push my
            // screen" action.
            system()->initializeDesktopCamera();
        };

    auto generalPageWidget = new QnGeneralPreferencesWidget(this);
    addPage(GeneralPage, generalPageWidget, tr("General"));
    connect(generalPageWidget, &QnGeneralPreferencesWidget::recordingSettingsChanged, this,
        updateRecorderSettings);

    addPage(LookAndFeelPage, m_lookAndFeelWidget, tr("Look and Feel"));

    const auto screenRecordingAction = action(menu::ToggleScreenRecordingAction);
    if (screenRecordingAction)
    {
        auto recordingSettingsWidget = new RecordingSettingsWidget(this);
        addPage(RecordingPage, recordingSettingsWidget, tr("Screen Recording"));
        connect(recordingSettingsWidget, &RecordingSettingsWidget::recordingSettingsChanged, this,
            updateRecorderSettings);
    }

    addPage(NotificationsPage, new QnPopupSettingsWidget(this), tr("Notifications"));
    addPage(AdvancedPage, m_advancedSettingsWidget, tr("Advanced"));

    setWarningStyle(ui->readOnlyWarningLabel);
    ui->readOnlyWarningWidget->setVisible(!appContext()->localSettings()->isWritable());
    ui->readOnlyWarningLabel->setText(
        tr("Settings are read-only. Please contact your system administrator. "
            "All changes will be lost after program exit.")
    );

    addRestartLabel();
}

QnLocalSettingsDialog::~QnLocalSettingsDialog() {}

bool QnLocalSettingsDialog::canApplyChanges() const
{
    return appContext()->localSettings()->isWritable() && base_type::canApplyChanges();
}

void QnLocalSettingsDialog::applyChanges()
{
    executeWithRestartCheck(
        [this]
        {
            base_type::applyChanges();
        });
}

void QnLocalSettingsDialog::updateButtonBox()
{
    base_type::updateButtonBox();
    m_restartLabel->setVisible(isRestartRequired());
}

void QnLocalSettingsDialog::accept()
{
    executeWithRestartCheck(
        [this]
        {
            base_type::accept();
        });
}

bool QnLocalSettingsDialog::isRestartRequired() const
{
    return m_advancedSettingsWidget->isRestartRequired()
        || m_lookAndFeelWidget->isRestartRequired();
}

void QnLocalSettingsDialog::addRestartLabel()
{
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(ui->buttonBox->layout());
    NX_ASSERT(layout, "Layout must already exist here.");
    if (!layout)
        return;

    m_restartLabel = new QLabel(tr("Restart required"), ui->buttonBox);
    setWarningStyle(m_restartLabel);
    m_restartLabel->setVisible(false);

    layout->insertWidget(0, m_restartLabel);
}

void QnLocalSettingsDialog::executeWithRestartCheck(Callback function) const
{
    // Method can be reentered, but dialog should appear only once.
    if (m_checkingRestart)
    {
        function();
        return;
    }
    QScopedValueRollback<bool> checkGuard(m_checkingRestart, true);

    bool restartQueued = false;

    if (isRestartRequired())
    {
        const auto answer = getRestartDialogAnswer();

        if (answer == QDialogButtonBox::Cancel)
            return;

        if (answer == QDialogButtonBox::Yes)
            restartQueued = true;
        // else: Do not restart, but make changes.
    }

    function();

    if (restartQueued)
        menu()->trigger(menu::QueueAppRestartAction);
}
