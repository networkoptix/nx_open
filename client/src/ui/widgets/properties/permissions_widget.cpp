#include "permissions_widget.h"
#include "ui_permissions_widget.h"

#include <ui/models/abstract_permissions_model.h>

namespace
{
    const char* kPermissionProperty = "_qn_global_permission_property";
}

QnPermissionsWidget::QnPermissionsWidget(QnAbstractPermissionsModel* permissionsModel, QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::PermissionsWidget()),
    m_permissionsModel(permissionsModel)
{
    NX_ASSERT(m_permissionsModel);
    ui->setupUi(this);

    auto createCheckBox = [this](Qn::GlobalPermission permission, const QString& text)
    {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(text);
        checkbox->setProperty(kPermissionProperty, qVariantFromValue(permission));
        ui->permissionsLayout->addWidget(checkbox);
        m_checkboxes << checkbox;

        connect(checkbox, &QCheckBox::clicked, this, &QnAbstractPreferencesWidget::hasChangesChanged);
        return checkbox;
    };

    /* Manager permissions. */
    createCheckBox(Qn::GlobalEditCamerasPermission,         tr("Can edit camera settings"));
    createCheckBox(Qn::GlobalControlVideoWallPermission,    tr("Can control videowalls"));
    createCheckBox(Qn::GlobalViewLogsPermission,            tr("Can view audit trail and event log"));

    /* Viewer permissions. */
    createCheckBox(Qn::GlobalViewArchivePermission,         tr("Can view archives of available cameras"));
    createCheckBox(Qn::GlobalExportPermission,              tr("Can export archives of available cameras"));
    createCheckBox(Qn::GlobalViewBookmarksPermission,       tr("Can view bookmarks of available cameras"));
    createCheckBox(Qn::GlobalManageBookmarksPermission,     tr("Can modify bookmarks of available cameras"));
    createCheckBox(Qn::GlobalUserInputPermission,           tr("Can change camera's PTZ state"));

    ui->permissionsLayout->addStretch();
}

QnPermissionsWidget::~QnPermissionsWidget()
{}

bool QnPermissionsWidget::hasChanges() const
{
    Qn::GlobalPermissions value = m_permissionsModel->rawPermissions();
    value &= ~Qn::GlobalAccessAllCamerasPermission; /*< This permission is handled separately. */

    return selectedPermissions() != value;
}

void QnPermissionsWidget::loadDataToUi()
{
    Qn::GlobalPermissions value = m_permissionsModel->rawPermissions();

    for (QCheckBox* checkbox : m_checkboxes)
    {
        Qn::GlobalPermission flag = checkbox->property(kPermissionProperty).value<Qn::GlobalPermission>();
        checkbox->setChecked(value.testFlag(flag));
    }
}

void QnPermissionsWidget::applyChanges()
{
    auto value = m_permissionsModel->rawPermissions();
    for (QCheckBox* checkbox : m_checkboxes)
    {
        Qn::GlobalPermission flag = checkbox->property(kPermissionProperty).value<Qn::GlobalPermission>();
        if (checkbox->isChecked())
            value |= flag;
        else
            value &= ~flag;
    }
    m_permissionsModel->setRawPermissions(value);
}

Qn::GlobalPermissions QnPermissionsWidget::selectedPermissions() const
{
    Qn::GlobalPermissions value = Qn::NoGlobalPermissions;
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
