#include "subject_target_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QPushButton>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>

#include <nx/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>

using namespace nx;
using namespace nx::client::desktop::ui;

QnSubjectTargetActionWidget::QnSubjectTargetActionWidget(QWidget* parent):
    base_type(parent),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
}

QnSubjectTargetActionWidget::~QnSubjectTargetActionWidget()
{
}

void QnSubjectTargetActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::actionParams))
        updateSubjectsButton();
}

void QnSubjectTargetActionWidget::selectSubjects()
{
    if (!model() || m_updating)
        return;

    SubjectSelectionDialog dialog(this);
    auto params = model()->actionParams();

    QSet<QnUuid> selected;
    for (auto id: params.additionalResources)
        selected << id;

    dialog.setCheckedSubjects(selected);
    dialog.setAllUsers(params.allUsers);

    const auto updateAlert =
        [&dialog]()
        {
            dialog.showAlert(!dialog.allUsers() && dialog.checkedSubjects().empty()
                ? vms::event::StringsHelper::needToSelectUserText()
                : QString());
        };

    connect(&dialog, &SubjectSelectionDialog::changed, this, updateAlert);
    updateAlert();

    if (dialog.exec() != QDialog::Accepted)
        return;

    {
        QScopedValueRollback<bool> updatingRollback(m_updating, true);

        params.allUsers = dialog.allUsers();
        if (!params.allUsers)
        {
            params.additionalResources.clear();

            for (const auto& id: dialog.checkedSubjects())
                params.additionalResources.push_back(id);
        }

        model()->setActionParams(params);
    }

    updateSubjectsButton();
}

void QnSubjectTargetActionWidget::setSubjectsButton(QPushButton* button)
{
    if (m_subjectsButton)
        m_subjectsButton->disconnect(this);

    m_subjectsButton = button;

    if (!m_subjectsButton)
        return;

    connect(m_subjectsButton, &QPushButton::clicked,
        this, &QnSubjectTargetActionWidget::selectSubjects);

    updateSubjectsButton();
}

void QnSubjectTargetActionWidget::updateSubjectsButton()
{
    if (!m_subjectsButton || !model())
        return;

    const auto icon =
        [](const QString& path) -> QIcon
        {
            static const QnIcon::SuffixesList suffixes {{ QnIcon::Normal, lit("selected") }};
            return qnSkin->icon(path, QString(), &suffixes);
        };

    const auto params = model()->actionParams();
    if (params.allUsers)
    {
        m_subjectsButton->setText(vms::event::StringsHelper::allUsersText());
        m_subjectsButton->setIcon(icon(lit("tree/users.png")));
    }
    else
    {
        QnUserResourceList users;
        QList<QnUuid> roles;
        userRolesManager()->usersAndRoles(params.additionalResources, users, roles);

        if (users.isEmpty() && roles.isEmpty())
        {
            m_subjectsButton->setText(vms::event::StringsHelper::needToSelectUserText());
            m_subjectsButton->setIcon(icon(lit("tree/user_alert.png")));
        }
        else
        {
            const bool multiple = users.size() > 1 || !roles.empty();
            m_subjectsButton->setText(m_helper->actionSubjects(users, roles));
            m_subjectsButton->setIcon(icon(multiple
                ? lit("tree/users.png")
                : lit("tree/user.png")));
        }
    }
}
