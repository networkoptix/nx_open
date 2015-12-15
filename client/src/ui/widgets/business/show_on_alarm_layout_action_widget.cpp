#include "show_on_alarm_layout_action_widget.h"
#include "ui_show_on_alarm_layout_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/common/scoped_value_rollback.h>


namespace {

    QnUserResourceList usersFromIds(const std::vector<QnUuid> &ids) {
        return qnResPool->getResources<QnUserResource>(ids);
    }

}

QnShowOnAlarmLayoutActionWidget::QnShowOnAlarmLayoutActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ShowOnAlarmLayoutActionWidget)
{
    ui->setupUi(this);

    connect(ui->forceOpenCheckBox,  &QCheckBox::clicked,    this,   &QnShowOnAlarmLayoutActionWidget::paramsChanged);
    connect(ui->useSourceCheckBox,  &QCheckBox::clicked,    this,   &QnShowOnAlarmLayoutActionWidget::paramsChanged);
    connect(ui->selectUsersButton,  &QPushButton::clicked,  this,   &QnShowOnAlarmLayoutActionWidget::selectUsers);
}

QnShowOnAlarmLayoutActionWidget::~QnShowOnAlarmLayoutActionWidget()
{}

void QnShowOnAlarmLayoutActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->forceOpenCheckBox);
    setTabOrder(ui->forceOpenCheckBox, ui->useSourceCheckBox);
    setTabOrder(ui->useSourceCheckBox, after);
}

void QnShowOnAlarmLayoutActionWidget::at_model_dataChanged(QnBusiness::Fields fields) {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(QnBusiness::ActionParamsField)) {
        ui->forceOpenCheckBox->setChecked(model()->actionParams().forced);
        ui->useSourceCheckBox->setChecked(model()->actionParams().useSource);
        updateUsersButtonText();
    }

    if (fields.testFlag(QnBusiness::EventTypeField)) {
        bool canUseSource = (model()->eventType() >= QnBusiness::UserDefinedEvent || requiresCameraResource(model()->eventType()));
        ui->useSourceCheckBox->setEnabled(canUseSource);
    }
}

void QnShowOnAlarmLayoutActionWidget::selectUsers() {
    if (!model() || m_updating)
        return;

    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::UserResourceTarget, this);
    dialog.setSelectedResources(usersFromIds(model()->actionParams().additionalResources));
    if (dialog.exec() != QDialog::Accepted)
        return;

    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

        std::vector<QnUuid> userIds;
        for (const QnUserResourcePtr &user: dialog.selectedResources().filtered<QnUserResource>())
            userIds.push_back(user->getId());

        QnBusinessActionParameters params = model()->actionParams();
        params.additionalResources = userIds;
        model()->setActionParams(params);
    }

    updateUsersButtonText();
}


void QnShowOnAlarmLayoutActionWidget::updateUsersButtonText() {
    QnUserResourceList users = model()
        ? usersFromIds(model()->actionParams().additionalResources)
        : QnUserResourceList();

    QString title;
    if (users.size() == 1)
        title = users.first()->getName();
    else if (users.isEmpty())
        title = tr("<All Users>");
    else
        title = tr("%n User(s)", "", users.size());

    ui->selectUsersButton->setText(title);
    ui->selectUsersButton->setIcon(qnResIconCache->icon(QnResourceIconCache::User));
}

void QnShowOnAlarmLayoutActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();
    params.forced = ui->forceOpenCheckBox->isChecked();
    params.useSource = ui->useSourceCheckBox->isChecked();
    model()->setActionParams(params);
}
