#include "permissions_widget.h"
#include "ui_permissions_widget.h"

#include <core/resource_management/resource_access_manager.h>

#include <ui/models/abstract_permissions_model.h>

namespace
{
    const char* kPermissionProperty = "_qn_global_permission_property";

    Qn::GlobalPermission permission(QCheckBox* checkbox)
    {
        return checkbox->property(kPermissionProperty).value<Qn::GlobalPermission>();
    }
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

        connect(checkbox, &QCheckBox::clicked, this, &QnPermissionsWidget::updateDependentPermissions);
        connect(checkbox, &QCheckBox::clicked, this, &QnAbstractPreferencesWidget::hasChangesChanged);
        return checkbox;
    };

    /* Manager permissions. */
    createCheckBox(Qn::GlobalEditCamerasPermission,         tr("Edit camera settings"));
    createCheckBox(Qn::GlobalControlVideoWallPermission,    tr("Control videowalls"));
    createCheckBox(Qn::GlobalViewLogsPermission,            tr("View event log"));

    /* Viewer permissions. */
    createCheckBox(Qn::GlobalViewArchivePermission,         tr("View archive"));
    createCheckBox(Qn::GlobalExportPermission,              tr("Export archive"));
    createCheckBox(Qn::GlobalViewBookmarksPermission,       tr("View bookmarks"));
    createCheckBox(Qn::GlobalManageBookmarksPermission,     tr("Modify bookmarks"));
    createCheckBox(Qn::GlobalUserInputPermission,           tr("User Input"));

    ui->permissionsLayout->addStretch();

    updateDependentPermissions();
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
        checkbox->setChecked(value.testFlag(permission(checkbox)));

    updateDependentPermissions();
}

void QnPermissionsWidget::applyChanges()
{
    auto value = m_permissionsModel->rawPermissions();
    for (QCheckBox* checkbox : m_checkboxes)
    {
        if (checkbox->isEnabled() && checkbox->isChecked())
            value |= permission(checkbox);
        else
            value &= ~permission(checkbox);
    }
    m_permissionsModel->setRawPermissions(value);
}

Qn::GlobalPermissions QnPermissionsWidget::selectedPermissions() const
{
    Qn::GlobalPermissions value = Qn::NoGlobalPermissions;
    for (QCheckBox* checkbox : m_checkboxes)
    {
        if (checkbox->isEnabled() && checkbox->isChecked())
            value |= permission(checkbox);
        else
            value &= ~permission(checkbox);
    }
    return value;
}

void QnPermissionsWidget::updateDependentPermissions()
{
    for (QCheckBox* checkbox : m_checkboxes)
    {
        auto dependent = QnResourceAccessManager::dependentPermissions(permission(checkbox));
        if (dependent == Qn::NoGlobalPermissions)
            continue;

        bool enable = checkbox->isEnabled() && checkbox->isChecked();
        for (QCheckBox* secondary : m_checkboxes)
        {
            if (dependent.testFlag(permission(secondary)))
                secondary->setEnabled(enable);
        }

    }
}
