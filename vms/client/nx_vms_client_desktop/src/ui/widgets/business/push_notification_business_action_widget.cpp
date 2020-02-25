#include "push_notification_business_action_widget.h"
#include "ui_push_notification_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include "ui/style/skin.h"

using namespace nx::vms::client::desktop::ui;

namespace nx::vms::client::desktop {

PushNotificationBusinessActionWidget::PushNotificationBusinessActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::PushNotificationBusinessActionWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::EventsActions_SendMobileNotification_Help);

    connect(ui->customTextCheckBox, &QCheckBox::clicked, this,
        [this]()
        {
            const bool useCustomText = ui->customTextCheckBox->isChecked();

            if (!useCustomText)
                m_lastCustomText = ui->customTextEdit->toPlainText();

            ui->customTextEdit->setPlainText(useCustomText ? m_lastCustomText : QString());

            parametersChanged();
        });
    connect(ui->customTextEdit, &QPlainTextEdit::textChanged,
        this, &PushNotificationBusinessActionWidget::parametersChanged);

    ui->customTextEdit->setEnabled(ui->customTextCheckBox->isChecked());
    connect(ui->customTextCheckBox, &QCheckBox::toggled, ui->customTextEdit, &QWidget::setEnabled);

    setSubjectsButton(ui->selectUsersButton);

    setValidationPolicy(new QnCloudUsersValidationPolicy(commonModule()));

    setDialogOptions(ui::SubjectSelectionDialog::CustomizableOptions::cloudUsers());
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

} // namespace nx::vms::client::desktop