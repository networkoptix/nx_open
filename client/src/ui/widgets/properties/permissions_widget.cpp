#include "permissions_widget.h"
#include "ui_permissions_widget.h"

#include <ui/delegates/abstract_permissions_delegate.h>

namespace
{
    const char* kPermissionProperty = "_qn_global_permission_property";
}

QnPermissionsWidget::QnPermissionsWidget(QnAbstractPermissionsDelegate* delegate, QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::PermissionsWidget()),
    m_delegate(delegate)
{
    NX_ASSERT(m_delegate);
    ui->setupUi(this);

    auto createCheckBox = [this](Qn::GlobalPermission permission, const QString& text)
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(text);
        checkbox->setProperty(kPermissionProperty, qVariantFromValue(permission));
        ui->permissionsLayout->addWidget(checkbox);
        m_checkboxes << checkbox;

        connect(checkbox, &QCheckBox::clicked, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    };

    createCheckBox(Qn::GlobalEditLayoutsPermission,     tr("Can create and edit global layouts"));
    createCheckBox(Qn::GlobalEditServersPermissions,    tr("Can edit server settings"));
    createCheckBox(Qn::GlobalViewLivePermission,        tr("Can view live stream of available cameras"));
    createCheckBox(Qn::GlobalViewArchivePermission,     tr("Can view archives of available cameras"));
    createCheckBox(Qn::GlobalExportPermission,          tr("Can export archives of available cameras"));
    createCheckBox(Qn::GlobalEditCamerasPermission,     tr("Can edit camera settings"));
    createCheckBox(Qn::GlobalPtzControlPermission,      tr("Can change camera's PTZ state"));
    createCheckBox(Qn::GlobalEditVideoWallPermission,   tr("Can create and edit videowalls"));
    ui->permissionsLayout->addStretch();
}

QnPermissionsWidget::~QnPermissionsWidget()
{}

bool QnPermissionsWidget::hasChanges() const
{
    return m_delegate->permissions() != calculatePermissions();
}

void QnPermissionsWidget::loadDataToUi()
{
    Qn::GlobalPermissions value = m_delegate->permissions();

    for (QCheckBox* checkbox : m_checkboxes)
    {
        Qn::GlobalPermission flag = checkbox->property(kPermissionProperty).value<Qn::GlobalPermission>();
        checkbox->setChecked(value.testFlag(flag));
    }
}

void QnPermissionsWidget::applyChanges()
{
    m_delegate->setPermissions(calculatePermissions());
}

Qn::GlobalPermissions QnPermissionsWidget::calculatePermissions() const
{
    Qn::GlobalPermissions value = m_delegate->permissions();

    for (QCheckBox* checkbox : m_checkboxes)
    {
        Qn::GlobalPermission flag = checkbox->property(kPermissionProperty).value<Qn::GlobalPermission>();

        if (checkbox->isChecked())
            value |= flag;
        else
            value &= ~flag;
    }
    return value;
}

//
//bool QnPermissionsWidget::targetIsValid() const
//{
//    /* Check if it is valid user id and we have access rights to edit it. */
//    if (m_targetUser)
//    {
//        Qn::Permissions permissions = accessController()->permissions(m_targetUser);
//        return permissions.testFlag(Qn::WriteAccessRightsPermission);
//    }
//
//    if (m_targetGroupId.isNull())
//        return false;
//
//    /* Only admins can edit groups. */
//    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
//        return false;
//
//    /* Check if it is valid user group id. */
//    const auto& userGroups = qnResourceAccessManager->userGroups();
//    return boost::algorithm::any_of(userGroups, [this](const ec2::ApiUserGroupData& group) { return group.id == m_targetGroupId; });
//}

