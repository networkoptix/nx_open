#include "show_on_alarm_layout_action_widget.h"
#include "ui_show_on_alarm_layout_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>

#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/common/scoped_value_rollback.h>

QnShowOnAlarmLayoutActionWidget::QnShowOnAlarmLayoutActionWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::ShowOnAlarmLayoutActionWidget)
{
    ui->setupUi(this);

    connect(ui->forceOpenCheckBox, &QCheckBox::clicked, this, &QnShowOnAlarmLayoutActionWidget::paramsChanged);
    connect(ui->useSourceCheckBox, &QCheckBox::clicked, this, &QnShowOnAlarmLayoutActionWidget::paramsChanged);
    connect(ui->selectUsersButton, &QPushButton::clicked, this, &QnShowOnAlarmLayoutActionWidget::selectUsers);
}

QnShowOnAlarmLayoutActionWidget::~QnShowOnAlarmLayoutActionWidget()
{
}

void QnShowOnAlarmLayoutActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->forceOpenCheckBox);
    setTabOrder(ui->forceOpenCheckBox, ui->useSourceCheckBox);
    setTabOrder(ui->useSourceCheckBox, after);
}

void QnShowOnAlarmLayoutActionWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(QnBusiness::ActionParamsField))
    {
        ui->forceOpenCheckBox->setChecked(model()->actionParams().forced);
        ui->useSourceCheckBox->setChecked(model()->actionParams().useSource);
        updateUsersButtonText();
    }

    if (fields.testFlag(QnBusiness::EventTypeField))
    {
        bool canUseSource = (model()->eventType() >= QnBusiness::UserDefinedEvent || requiresCameraResource(model()->eventType()));
        ui->useSourceCheckBox->setEnabled(canUseSource);
    }
}

void QnShowOnAlarmLayoutActionWidget::selectUsers()
{
    if (!model() || m_updating)
        return;

    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::users, this);
    QSet<QnUuid> selected;
    for (auto id : model()->actionParams().additionalResources)
        selected << id;
    dialog.setSelectedResources(selected);
    if (dialog.exec() != QDialog::Accepted)
        return;

    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

        std::vector<QnUuid> userIds;
        for (const auto &id : dialog.selectedResources())
            userIds.push_back(id);

        QnBusinessActionParameters params = model()->actionParams();
        params.additionalResources = userIds;
        model()->setActionParams(params);
    }

    updateUsersButtonText();
}


void QnShowOnAlarmLayoutActionWidget::updateUsersButtonText()
{
    auto ids = model()->actionParams().additionalResources;
    auto users = resourcePool()->getResources<QnUserResource>(ids);
    auto roles = userRolesManager()->userRoles(ids);

    QString title;
    if (users.size() == 1 && roles.empty())
        title = users.front()->getName();
    else if (users.empty() && roles.size() == 1)
        title = roles.front().name;
    else if (users.empty() && roles.empty())
        title = L'<' + tr("All Users") + L'>';
    else if (roles.empty())
        title = tr("%n Users", "", users.size());
    else if (users.empty())
        title = tr("%n Roles", "", (int)roles.size());
    else
        title = tr("%n Users", "", users.size()) + lit(", ") + tr("%n Roles", "", (int)roles.size());

    ui->selectUsersButton->setText(title);
    ui->selectUsersButton->setIcon(qnResIconCache->icon(QnResourceIconCache::User));
}

void QnShowOnAlarmLayoutActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();
    params.forced = ui->forceOpenCheckBox->isChecked();
    params.useSource = ui->useSourceCheckBox->isChecked();
    model()->setActionParams(params);
}
