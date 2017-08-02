#include "permissions_widget.h"
#include "ui_permissions_widget.h"

#include <QtWidgets/QLabel>

#include <core/resource_access/global_permissions_manager.h>

#include <ui/models/abstract_permissions_model.h>

namespace {

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

    // TODO: #GDM think where to store flags set to avoid duplication

    /* Manager permissions. */
    addCheckBox(Qn::GlobalEditCamerasPermission,         tr("Edit camera settings"),
        tr("This is also required to create/edit PTZ presets and tours."));
    addCheckBox(Qn::GlobalControlVideoWallPermission,    tr("Control video walls"));
    addCheckBox(Qn::GlobalViewLogsPermission,            tr("View event log"));

    /* Viewer permissions. */
    addCheckBox(Qn::GlobalViewArchivePermission,         tr("View archive"));
    addCheckBox(Qn::GlobalExportPermission,              tr("Export archive"));
    addCheckBox(Qn::GlobalViewBookmarksPermission,       tr("View bookmarks"));
    addCheckBox(Qn::GlobalManageBookmarksPermission,     tr("Modify bookmarks"));
    addCheckBox(Qn::GlobalUserInputPermission,           tr("User Input"),
        tr("PTZ, Device Output, 2-Way Audio, Soft Triggers."));

    ui->permissionsLayout->addStretch();

    updateDependentPermissions();
}

QnPermissionsWidget::~QnPermissionsWidget()
{}

bool QnPermissionsWidget::hasChanges() const
{
    Qn::GlobalPermissions value = m_permissionsModel->rawPermissions();

    /* These permissions are handled separately. */
    value &= ~Qn::GlobalAccessAllMediaPermission;

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

    /* We must set special 'Custom' flag for the users to avoid collisions with built-in roles. */
    if (m_permissionsModel->subject().user())
        value |= Qn::GlobalCustomUserPermission;

    return value;
}

void QnPermissionsWidget::updateDependentPermissions()
{
    for (QCheckBox* checkbox : m_checkboxes)
    {
        auto dependent = QnGlobalPermissionsManager::dependentPermissions(permission(checkbox));
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

void QnPermissionsWidget::addCheckBox(Qn::GlobalPermission permission, const QString& text, const QString& description /*= QString()*/)
{
    QCheckBox* checkbox = new QCheckBox(this);
    checkbox->setText(text);
    checkbox->setProperty(kPermissionProperty, qVariantFromValue(permission));
    m_checkboxes << checkbox;

    if (description.isEmpty())
    {
        ui->permissionsLayout->addWidget(checkbox);
    }
    else
    {
        auto layout = new QHBoxLayout();
        layout->addWidget(checkbox);
        layout->addWidget(new QLabel(description, this), 0x1000);
        ui->permissionsLayout->addLayout(layout);
    }

    connect(checkbox, &QCheckBox::clicked, this, &QnPermissionsWidget::updateDependentPermissions);
    connect(checkbox, &QCheckBox::clicked, this, &QnAbstractPreferencesWidget::hasChangesChanged);
}
