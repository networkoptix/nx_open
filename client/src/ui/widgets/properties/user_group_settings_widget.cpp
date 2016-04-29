#include "user_group_settings_widget.h"
#include "ui_user_group_settings_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/user_settings_model.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/email/email.h>

namespace
{
    const int kUserGroupIdRole = Qt::UserRole + 1;
    const int kPermissionsRole = Qt::UserRole + 2;
}

QnUserGroupSettingsWidget::QnUserGroupSettingsWidget(QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserGroupSettingsWidget())
{
    ui->setupUi(this);

}

QnUserGroupSettingsWidget::~QnUserGroupSettingsWidget()
{}

bool QnUserGroupSettingsWidget::hasChanges() const
{
    return false;
}

void QnUserGroupSettingsWidget::loadDataToUi()
{

}

void QnUserGroupSettingsWidget::applyChanges()
{

}
