#include "subject_target_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QPushButton>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

QnSubjectTargetActionWidget::QnSubjectTargetActionWidget(QWidget* parent):
    base_type(parent),
    m_helper(new QnBusinessStringsHelper(commonModule()))
{
}

QnSubjectTargetActionWidget::~QnSubjectTargetActionWidget()
{
}

void QnSubjectTargetActionWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(QnBusiness::ActionParamsField))
        updateSubjectsButton();
}

void QnSubjectTargetActionWidget::selectSubjects()
{
    if (!model() || m_updating)
        return;

    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::users, this);
    QSet<QnUuid> selected;
    for (auto id: model()->actionParams().additionalResources)
        selected << id;

    dialog.setSelectedResources(selected);
    if (dialog.exec() != QDialog::Accepted)
        return;

    {
        QScopedValueRollback<bool> updatingRollback(m_updating, true);

        std::vector<QnUuid> userIds;
        for (const auto &id: dialog.selectedResources())
            userIds.push_back(id);

        QnBusinessActionParameters params = model()->actionParams();
        params.additionalResources = userIds;
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

    QnUserResourceList users;
    QList<QnUuid> roles;
    userRolesManager()->usersAndRoles(model()->actionParams().additionalResources, users, roles);

    if (m_requiresExplicitSelection && users.isEmpty() && roles.isEmpty())
    {
        m_subjectsButton->setText(tr("Select at least one user..."));
        m_subjectsButton->setIcon(qnSkin->icon(lit("tree/user_alert.png")));
    }
    else
    {
        m_subjectsButton->setText(m_helper->actionSubjects(users, roles));
        m_subjectsButton->setIcon(qnResIconCache->icon(users.size() > 1 || !roles.empty()
            ? QnResourceIconCache::Users
            : QnResourceIconCache::User));
    }
}

bool QnSubjectTargetActionWidget::requiresExplicitSelection() const
{
    return m_requiresExplicitSelection;
}

void QnSubjectTargetActionWidget::setRequiresExplicitSelection(bool value)
{
    if (m_requiresExplicitSelection == value)
        return;

    m_requiresExplicitSelection = value;
    updateSubjectsButton();
}
