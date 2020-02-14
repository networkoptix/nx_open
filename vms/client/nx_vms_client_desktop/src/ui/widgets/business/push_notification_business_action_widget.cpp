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

    setSubjectsButton(ui->selectUsersButton);

    setValidationPolicy(new QnCloudUsersValidationPolicy(commonModule()));

    nx::vms::client::desktop::ui::SubjectSelectionDialog::CustomizableOptions options;
    options.userListHeader = tr("Cloud users");
    options.userFilter =
        [](const QnUserResource& user) -> bool
        {
            return user.isCloud();
        };
    setDialogOptions(options);
}

PushNotificationBusinessActionWidget::~PushNotificationBusinessActionWidget()
{
}

void PushNotificationBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    //if (fields.testFlag(Field::actionParams))
    //    ui->forceAcknoledgementCheckBox->setChecked(model()->actionParams().needConfirmation);

    base_type::at_model_dataChanged(fields);
}

void PushNotificationBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, after);
}

void PushNotificationBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    //auto params = model()->actionParams();
    //params.needConfirmation = ui->forceAcknoledgementCheckBox->isChecked();
    //model()->setActionParams(params);
    updateSubjectsButton();
}

} // namespace nx::vms::client::desktop