#include "software_trigger_business_event_widget.h"
#include "ui_software_trigger_business_event_widget.h"

#include <QtCore/QtMath>
#include <QtCore/QScopedValueRollback>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <ui/common/aligner.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/event/strings_helper.h>

using namespace nx;
using namespace nx::client::desktop::ui;

namespace {

static constexpr int kDropdownIconSize = 40;

} // namespace

QnSoftwareTriggerBusinessEventWidget::QnSoftwareTriggerBusinessEventWidget(QWidget* parent) :
    base_type(parent),
    ui(new Ui::SoftwareTriggerBusinessEventWidget),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
    ui->setupUi(this);

    ui->usersButton->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->triggerIdLineEdit->setPlaceholderText(
        nx::vms::event::StringsHelper::defaultSoftwareTriggerName());

    connect(ui->triggerIdLineEdit, &QLineEdit::textChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->iconComboBox, QnComboboxCurrentIndexChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->usersButton, &QPushButton::clicked, this,
        &QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked);

    const auto nextEvenValue =
        [](int value) { return value + (value & 1); };

    const auto columnCount = nextEvenValue(qCeil(qSqrt(
        QnSoftwareTriggerPixmaps::pixmapNames().size())));

    ui->iconComboBox->setColumnCount(columnCount);

    ui->iconComboBox->setItemSize({ kDropdownIconSize, kDropdownIconSize });

    ui->iconComboBox->setPixmaps(
        QnSoftwareTriggerPixmaps::pixmapsPath(),
        QnSoftwareTriggerPixmaps::pixmapNames());

    auto aligner = new QnAligner(this);
    aligner->addWidget(ui->nameLabel);
    aligner->addWidget(ui->iconLabel);
    aligner->addWidget(ui->usersLabel);
}

QnSoftwareTriggerBusinessEventWidget::~QnSoftwareTriggerBusinessEventWidget()
{
}

void QnSoftwareTriggerBusinessEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->usersButton);
    setTabOrder(ui->iconComboBox, after);
}

void QnSoftwareTriggerBusinessEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::eventParams))
    {
        const auto params = model()->eventParams();
        ui->triggerIdLineEdit->setText(params.caption);
        ui->iconComboBox->setCurrentIcon(
            QnSoftwareTriggerPixmaps::effectivePixmapName(params.description));

        updateUsersButton();
    }
}

void QnSoftwareTriggerBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto eventParams = model()->eventParams();

    eventParams.caption = ui->triggerIdLineEdit->text().trimmed();
    eventParams.description = ui->iconComboBox->currentIcon();

    model()->setEventParams(eventParams);
}

bool QnSoftwareTriggerBusinessEventWidget::isRoleValid(const QnUuid& id) const
{
    // TODO: #vkutin #3.2 Move it to some validation policy, old-style or new.

    const auto role = userRolesManager()->predefinedRole(id);
    switch (role)
    {
        case Qn::UserRole::CustomPermissions:
        {
            NX_ASSERT(false); //< Should never happen.
            return false;
        }

        case Qn::UserRole::CustomUserRole:
        {
            const auto customRole = userRolesManager()->userRole(id);
            return customRole.permissions.testFlag(Qn::GlobalUserInputPermission);
        }

        default:
        {
            const auto permissions = userRolesManager()->userRolePermissions(role);
            return permissions.testFlag(Qn::GlobalUserInputPermission);
        }
    }
}

bool QnSoftwareTriggerBusinessEventWidget::isUserValid(const QnUserResourcePtr& user) const
{
    // TODO: #vkutin #3.2 Move it to some validation policy, old-style or new.
    return resourceAccessManager()->hasGlobalPermission(user, Qn::GlobalUserInputPermission);
}

QString QnSoftwareTriggerBusinessEventWidget::calculateAlert(
    bool allUsers, const QSet<QnUuid>& checkedSubjects) const
{
    // TODO: #vkutin #3.2 Move it to some validation policy, old-style or new.

    static const auto kHtmlTemplate = lit("<html>%1</html>");

    if (!allUsers && checkedSubjects.empty())
        return kHtmlTemplate.arg(vms::event::StringsHelper::needToSelectUserText());

    QnUserResourceList users;
    QList<QnUuid> roleIds;

    if (allUsers)
        users = resourcePool()->getResources<QnUserResource>();
    else
        userRolesManager()->usersAndRoles(checkedSubjects, users, roleIds);

    // Analyze only enabled users.
    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

    QList<QnUuid> invalidRoles;
    for (const auto& id: roleIds)
    {
        if (!isRoleValid(id))
            invalidRoles << id;
    }

    QString alert;
    if (invalidRoles.size() > 0)
    {
        if (invalidRoles.size() == 1)
        {
            alert = tr("Role %1 has no User Input permission",
                "%1 is the name of selected role")
                .arg(lit("<b>%1</b>").arg(userRolesManager()->userRoleName(invalidRoles.front())));
        }
        else if (invalidRoles.size() == roleIds.size())
        {
            alert = tr("Selected roles have no User Input permission");
        }
        else
        {
            alert = tr("%n of %1 selected roles have no User Input permission",
                "%1 is number of selected roles", invalidRoles.size()).arg(roleIds.size());
        }
    }

    const auto invalidUsers = users.filtered(
        [this](const QnUserResourcePtr& user) { return !isUserValid(user); });

    if (invalidUsers.size() > 0)
    {
        if (!alert.isEmpty())
            alert += lit("<br>");

        if (invalidUsers.size() == 1)
        {
            alert += tr("User %1 has no User Input permission",
                "%1 is the name of selected user")
                .arg(lit("<b>%1</b>").arg(invalidUsers.front()->getName()));
        }
        else if (invalidUsers.size() == users.size())
        {
            alert += tr("Selected users have no User Input permission");
        }
        else
        {
            alert += tr("%n of %1 selected users have no User Input permission",
                "%1 is number of selected users", invalidUsers.size()).arg(users.size());
        }
    }

    return alert.isEmpty() ? alert : kHtmlTemplate.arg(alert);
}

void QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked()
{
    SubjectSelectionDialog dialog(this);
    auto params = model()->eventParams();

    QSet<QnUuid> selected;
    for (const auto& id: params.metadata.instigators)
        selected.insert(id);

    const auto roleValidator =
        [this](const QnUuid& roleId)
        {
            return isRoleValid(roleId)
                ? QValidator::Acceptable
                : QValidator::Invalid;
        };

    const auto userValidator =
        [this](const QnUserResourcePtr& user) { return isUserValid(user); };

    dialog.setCheckedSubjects(selected);
    dialog.setAllUsers(params.metadata.allUsers);
    dialog.setRoleValidator(roleValidator);
    dialog.setUserValidator(userValidator);

    const auto updateAlert =
        [this, &dialog]()
        {
            dialog.showAlert(calculateAlert(dialog.allUsers(), dialog.checkedSubjects()));
        };

    connect(&dialog, &SubjectSelectionDialog::changed, this, updateAlert);
    updateAlert();

    if (dialog.exec() != QDialog::Accepted)
        return;

    params.metadata.allUsers = dialog.allUsers();
    if (!params.metadata.allUsers)
    {
        selected = dialog.checkedSubjects();
        params.metadata.instigators.clear();
        params.metadata.instigators.reserve(selected.size());
        params.metadata.instigators.insert(params.metadata.instigators.end(),
            selected.constBegin(), selected.constEnd());
    }

    model()->setEventParams(params);
    updateUsersButton();
}

void QnSoftwareTriggerBusinessEventWidget::updateUsersButton()
{
    const auto params = model()->eventParams();

    const auto icon =
        [](const QString& path) -> QIcon
        {
            static const QnIcon::SuffixesList suffixes {{ QnIcon::Normal, lit("selected") }};
            return qnSkin->icon(path, QString(), &suffixes);
        };

    if (params.metadata.allUsers)
    {
        const auto users = resourcePool()->getResources<QnUserResource>()
            .filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

        const bool allValid =
            std::all_of(users.begin(), users.end(),
                [this](const QnUserResourcePtr& user) { return isUserValid(user); });

        ui->usersButton->setText(vms::event::StringsHelper::allUsersText());
        ui->usersButton->setIcon(icon(allValid
            ? lit("tree/users.png")
            : lit("tree/users_alert.png")));
    }
    else
    {
        QnUserResourceList users;
        QList<QnUuid> roles;
        userRolesManager()->usersAndRoles(params.metadata.instigators, users, roles);
        users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

        if (users.isEmpty() && roles.isEmpty())
        {
            ui->usersButton->setText(vms::event::StringsHelper::needToSelectUserText());
            ui->usersButton->setIcon(qnSkin->icon(lit("tree/user_alert.png")));
        }
        else
        {
            const bool allValid =
                std::all_of(users.begin(), users.end(),
                    [this](const QnUserResourcePtr& user) { return isUserValid(user); })
                && std::all_of(roles.begin(), roles.end(),
                    [this](const QnUuid& roleId) { return isRoleValid(roleId); });

            // TODO: #vkutin #3.2 Color the button red if selection is completely invalid.

            const bool multiple = users.size() > 1 || !roles.empty();
            ui->usersButton->setText(m_helper->actionSubjects(users, roles));
            ui->usersButton->setIcon(icon(multiple
                ? (allValid ? lit("tree/users.png") : lit("tree/users_alert.png"))
                : (allValid ? lit("tree/user.png") : lit("tree/user_alert.png"))));
        }
    }
}
