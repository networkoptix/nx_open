#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workbench/workbench_context.h>

#include <business/business_resource_validation.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include "ui/style/skin.h"

using namespace nx::vms::client::desktop::ui;

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupBusinessActionWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::EventsActions_ShowNotification_Help);

    ui->hintLabel->setHint(tr("Notification will be shown until one of the users who see it "
        "creates bookmark with event description"));

    connect(ui->settingsButton, &QPushButton::clicked,
        this, &QnPopupBusinessActionWidget::at_settingsButton_clicked);
    connect(ui->forceAcknoledgementCheckBox, &QCheckBox::toggled,
        this, &QnPopupBusinessActionWidget::parametersChanged);

    setSubjectsButton(ui->selectUsersButton);
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
}

void QnPopupBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    if (fields.testFlag(Field::eventType))
    {
        const auto sourceCameraRequired =
            nx::vms::event::isSourceCameraRequired(model()->eventType());
        const auto allowForceAcknoledgement = sourceCameraRequired
            || model()->eventType() >= nx::vms::api::EventType::userDefinedEvent;
        ui->forceAcknoledgementCheckBox->setEnabled(allowForceAcknoledgement);
        ui->hintLabel->setEnabled(allowForceAcknoledgement);
    }

    if (fields.testFlag(Field::actionParams))
        ui->forceAcknoledgementCheckBox->setChecked(model()->actionParams().needConfirmation);

    updateValidationPolicy();
    base_type::at_model_dataChanged(fields);
}

void QnPopupBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->settingsButton);
    setTabOrder(ui->settingsButton, ui->forceAcknoledgementCheckBox);
    setTabOrder(ui->forceAcknoledgementCheckBox, after);
}

void QnPopupBusinessActionWidget::at_settingsButton_clicked()
{
    menu()->trigger(action::PreferencesNotificationTabAction);
}

void QnPopupBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto params = model()->actionParams();
    params.needConfirmation = ui->forceAcknoledgementCheckBox->isChecked();
    model()->setActionParams(params);
    updateValidationPolicy();
    updateSubjectsButton();
}

void QnPopupBusinessActionWidget::updateValidationPolicy()
{
    const bool forceAcknowledgement = ui->forceAcknoledgementCheckBox->isEnabled()
        && ui->forceAcknoledgementCheckBox->isChecked();

    if (forceAcknowledgement)
    {
        setValidationPolicy(new QnRequiredPermissionSubjectPolicy(
            GlobalPermission::manageBookmarks, tr("Manage Bookmarks")));
    }
    else
    {
        setValidationPolicy(new QnDefaultSubjectValidationPolicy());
    }
}
