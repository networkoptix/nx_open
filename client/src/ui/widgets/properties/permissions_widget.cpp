#include "permissions_widget.h"
#include "ui_permissions_widget.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <core/resource/user_resource.h>

#include <ui/workbench/workbench_access_controller.h>

QnPermissionsWidget::QnPermissionsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PermissionsWidget()),
    m_targetGroupId(),
    m_targetUser()
{
    ui->setupUi(this);

}

QnPermissionsWidget::~QnPermissionsWidget()
{

}


QnUuid QnPermissionsWidget::targetGroupId() const
{
    return m_targetGroupId;
}

void QnPermissionsWidget::setTargetGroupId(const QnUuid& id)
{
    NX_ASSERT(m_targetUser.isNull());
    m_targetGroupId = id;
    NX_ASSERT(m_targetGroupId.isNull() || targetIsValid());
}

QnUserResourcePtr QnPermissionsWidget::targetUser() const
{
    return m_targetUser;
}

void QnPermissionsWidget::setTargetUser(const QnUserResourcePtr& user)
{
    NX_ASSERT(m_targetGroupId.isNull());
    m_targetUser = user;
    NX_ASSERT(!user || targetIsValid());
}

bool QnPermissionsWidget::hasChanges() const
{
    /* Validate target. */
    if (!targetIsValid())
        return false;

    return false;
}

void QnPermissionsWidget::loadDataToUi()
{
    /* Validate target. */
    if (!targetIsValid())
        return;
}

void QnPermissionsWidget::applyChanges()
{
    /* Validate target. */
    if (!targetIsValid())
        return;

}

bool QnPermissionsWidget::targetIsValid() const
{
    /* Check if it is valid user id and we have access rights to edit it. */
    if (m_targetUser)
    {
        Qn::Permissions permissions = accessController()->permissions(m_targetUser);
        return permissions.testFlag(Qn::WriteAccessRightsPermission);
    }

    if (m_targetGroupId.isNull())
        return false;

    /* Only admins can edit groups. */
    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return false;

    /* Check if it is valid user group id. */
    const auto& userGroups = qnResourceAccessManager->userGroups();
    return boost::algorithm::any_of(userGroups, [this](const ec2::ApiUserGroupData& group) { return group.id == m_targetGroupId; });
}

QnUuid QnPermissionsWidget::targetId() const
{
    return m_targetUser
        ? m_targetUser->getId()
        : m_targetGroupId;
}

