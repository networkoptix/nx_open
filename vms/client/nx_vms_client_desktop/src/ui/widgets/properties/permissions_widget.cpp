#include "permissions_widget.h"
#include "ui_permissions_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>

#include <core/resource_access/global_permissions_manager.h>

#include <ui/models/abstract_permissions_model.h>

namespace {

const char* kPermissionProperty = "_qn_global_permission_property";

GlobalPermission permission(QCheckBox* checkbox)
{
    return checkbox->property(kPermissionProperty).value<GlobalPermission>();
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
    addCheckBox(GlobalPermission::editCameras,         tr("Edit camera settings"),
        tr("This is also required to create/edit PTZ presets and tours."));
    addCheckBox(GlobalPermission::controlVideowall,    tr("Control video walls"));
    addCheckBox(GlobalPermission::viewLogs,            tr("View event log"));

    /* Viewer permissions. */
    addCheckBox(GlobalPermission::viewArchive,         tr("View archive"));
    addCheckBox(GlobalPermission::exportArchive,              tr("Export archive"));
    addCheckBox(GlobalPermission::viewBookmarks,       tr("View bookmarks"));
    addCheckBox(GlobalPermission::manageBookmarks,     tr("Modify bookmarks"));
    addCheckBox(GlobalPermission::userInput,           tr("User Input"),
        tr("PTZ, Device Output, 2-Way Audio, Soft Triggers."));

    ui->permissionsLayout->addStretch();

    updateDependentPermissions();
}

QnPermissionsWidget::~QnPermissionsWidget()
{}

bool QnPermissionsWidget::hasChanges() const
{
    GlobalPermissions value = m_permissionsModel->rawPermissions();

    /* These permissions are handled separately. */
    value &= ~GlobalPermissions(GlobalPermission::accessAllMedia);

    return selectedPermissions() != value;
}

void QnPermissionsWidget::loadDataToUi()
{
    GlobalPermissions value = m_permissionsModel->rawPermissions();

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
            value &= ~GlobalPermissions(permission(checkbox));
    }

    m_permissionsModel->setRawPermissions(value);
}

GlobalPermissions QnPermissionsWidget::selectedPermissions() const
{
    GlobalPermissions value;
    for (QCheckBox* checkbox : m_checkboxes)
    {
        if (checkbox->isEnabled() && checkbox->isChecked())
            value |= permission(checkbox);
        else
            value &= ~GlobalPermissions(permission(checkbox));
    }

    /* We must set special 'Custom' flag for the users to avoid collisions with built-in roles. */
    if (m_permissionsModel->subject().user())
        value |= GlobalPermission::customUser;

    return value;
}

void QnPermissionsWidget::updateDependentPermissions()
{
    for (QCheckBox* checkbox : m_checkboxes)
    {
        auto dependent = QnGlobalPermissionsManager::dependentPermissions(permission(checkbox));
        if (dependent == 0)
            continue;

        bool enable = checkbox->isEnabled() && checkbox->isChecked();
        for (QCheckBox* secondary : m_checkboxes)
        {
            if (dependent.testFlag(permission(secondary)))
                secondary->setEnabled(enable);
        }

    }
}

void QnPermissionsWidget::addCheckBox(GlobalPermission permission,
    const QString& text, const QString& description /*= QString()*/)
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
