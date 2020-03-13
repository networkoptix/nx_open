#include "push_notification_business_action_widget.h"
#include "ui_push_notification_business_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <api/global_settings.h>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/style/skin.h>

#include <nx/network/app_info.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop::ui;

namespace {

class Filter: public QObject
{
public:
    Filter(const std::function<void()>& action, QObject* parent = nullptr):
        QObject(parent),
        m_action(action)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonRelease)
            m_action();

        return false;
    }

private:
    const std::function<void()> m_action;
};

} // namespace

namespace nx::vms::client::desktop {

PushNotificationBusinessActionWidget::PushNotificationBusinessActionWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PushNotificationBusinessActionWidget)
{
    ui->setupUi(this);

    ui->notConnectedLabel->setText(
        tr("The system is not connected to %1. "
            "Mobile notifications work only when the system is connected to %1.",
            "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
            .arg(nx::network::AppInfo::cloudName()));

    ui->cloudSettingsButton->setText(
        tr("%1 Settings", "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
            .arg(nx::network::AppInfo::cloudName()));

    setHelpTopic(this, Qn::EventsActions_SendMobileNotification_Help);

    ui->languageButton->setIcon(qnSkin->pixmap("events/lang.svg"));
    connect(ui->languageButton, &QPushButton::clicked, this,
        [this]
        {
            context()->menu()->trigger(ui::action::SystemAdministrationAction);
        });

    connect(ui->customTextCheckBox, &QCheckBox::clicked, this,
        [this]()
        {
            const bool useCustomText = ui->customTextCheckBox->isChecked();

            if (!useCustomText)
                m_lastCustomText = ui->customTextEdit->toPlainText();

            ui->customTextEdit->setPlainText(useCustomText ? m_lastCustomText : QString());

            parametersChanged();
        });
    ui->customTextEdit->installEventFilter(new Filter(
        [this]
        {
            if (!ui->customTextCheckBox->isChecked())
            {
                ui->customTextCheckBox->setChecked(true);
                ui->customTextEdit->setPlainText(m_lastCustomText);
                ui->customTextEdit->setFocus(Qt::MouseFocusReason);
                // TODO: set text cursor under the mouse cursor?
            }
        },
        this));

    connect(ui->customTextCheckBox, &QCheckBox::toggled, ui->customTextEdit, &QWidget::setEnabled);

    connect(ui->customTextEdit, &QPlainTextEdit::textChanged,
        this, &PushNotificationBusinessActionWidget::parametersChanged);

    setSubjectsButton(ui->selectUsersButton);

    setValidationPolicy(new QnCloudUsersValidationPolicy());

    setDialogOptions(ui::SubjectSelectionDialog::CustomizableOptions::cloudUsers());

    connect(
        ui->cloudSettingsButton,
        &QPushButton::clicked,
        action(action::PreferencesCloudTabAction),
        &QAction::triggered);

    connect(
        base_type::qnGlobalSettings,
        &QnGlobalSettings::cloudSettingsChanged,
        this,
        &PushNotificationBusinessActionWidget::updateCurrentTab);
    updateCurrentTab();
}

PushNotificationBusinessActionWidget::~PushNotificationBusinessActionWidget()
{
}

void PushNotificationBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();

        const bool useCustomText = !params.text.isEmpty();
        m_lastCustomText = params.text;
        ui->customTextCheckBox->setChecked(useCustomText);
        ui->customTextEdit->setPlainText(useCustomText ? params.text : QString());
    }

    base_type::at_model_dataChanged(fields);
}

void PushNotificationBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->customTextCheckBox);
    setTabOrder(ui->customTextCheckBox, ui->customTextEdit);
    setTabOrder(ui->customTextEdit, after);
}

void PushNotificationBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto params = model()->actionParams();
    params.text = ui->customTextCheckBox->isChecked() ? ui->customTextEdit->toPlainText() : QString();
    model()->setActionParams(params);
    updateSubjectsButton();
}

void PushNotificationBusinessActionWidget::updateCurrentTab()
{
    ui->stackedWidget->setCurrentIndex(
        base_type::qnGlobalSettings->cloudSystemId().isEmpty() ? 0 : 1);
}

} // namespace nx::vms::client::desktop