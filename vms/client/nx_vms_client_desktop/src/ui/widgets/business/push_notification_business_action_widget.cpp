#include "push_notification_business_action_widget.h"
#include "ui_push_notification_business_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <api/global_settings.h>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>

#include <nx/vms/client/desktop/common/utils/aligner.h>

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
    ui(new Ui::PushNotificationBusinessActionWidget),
    m_aligner(new Aligner(this))
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

    ui->languageButton->setIcon(qnSkin->icon("events/push_language.svg"));
    connect(ui->languageButton, &QPushButton::clicked, this,
        [this]
        {
            context()->menu()->trigger(ui::action::SystemAdministrationAction);
        });

    m_aligner->addWidgets(
        {ui->usersLabel, ui->dummyLabel, ui->headerLabel, ui->bodyLabel, ui->dummyLabel2});

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

    connect(ui->customContentCheckBox, &QCheckBox::toggled, this,
        [this](bool useCustomContent)
        {
            ui->customContentWidget->setVisible(useCustomContent);
            parametersChanged();
        });
    ui->customContentWidget->setVisible(ui->customContentCheckBox->isChecked());

    connect(ui->headerText, &QLineEdit::textChanged,
        this, &PushNotificationBusinessActionWidget::parametersChanged);
    connect(ui->bodyText, &QPlainTextEdit::textChanged,
        this, &PushNotificationBusinessActionWidget::parametersChanged);
    connect(ui->addSourceNameCheckBox, &QCheckBox::clicked,
        this, &PushNotificationBusinessActionWidget::parametersChanged);

    // TODO: #spanasenko We need to initialize model with the right value instead.
    ui->addSourceNameCheckBox->setChecked(true);
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
        QScopedValueRollback<bool> guard(m_updating, true);
        const auto params = model()->actionParams();

        const bool useCustomContent = !params.useSource
            || !params.text.isEmpty() || !params.sayText.isEmpty();

        ui->headerText->setText(params.sayText);
        ui->bodyText->setPlainText(params.text);
        ui->addSourceNameCheckBox->setChecked(params.useSource);
        ui->customContentCheckBox->setChecked(useCustomContent);
    }

    base_type::at_model_dataChanged(fields);
}

void PushNotificationBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->languageButton);
    setTabOrder(ui->languageButton, ui->customContentCheckBox);
    setTabOrder(ui->customContentCheckBox, ui->headerText);
    setTabOrder(ui->headerText, ui->bodyText);
    setTabOrder(ui->bodyText, ui->addSourceNameCheckBox);
    setTabOrder(ui->addSourceNameCheckBox, after);
}

void PushNotificationBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto params = model()->actionParams();

    const bool useCustomContent = ui->customContentCheckBox->isChecked();

    params.sayText = useCustomContent ? ui->headerText->text() : QString();
    params.text = useCustomContent ? ui->bodyText->toPlainText() : QString();
    params.useSource = useCustomContent ? ui->addSourceNameCheckBox->isChecked() : true;
    model()->setActionParams(params);

    updateSubjectsButton();
}

void PushNotificationBusinessActionWidget::updateCurrentTab()
{
    ui->stackedWidget->setCurrentIndex(
        base_type::qnGlobalSettings->cloudSystemId().isEmpty() ? 0 : 1);
}

} // namespace nx::vms::client::desktop