#include "select_users_delegate_editor_button.h"

#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <core/resource/user_resource.h>
#include <business/business_resource_validation.h>
#include <business/business_types_comparator.h>
#include <ui/models/business_rules_view_model.h>

using namespace nx;
using namespace nx::vms::client::desktop::ui;

QnSelectUsersDialogButton::QnSelectUsersDialogButton(QWidget* parent):
    base_type(parent),
    m_dialogDelegate(nullptr)
{
}

QnSelectUsersDialogButton::~QnSelectUsersDialogButton()
{
}

QnResourceSelectionDialogDelegate* QnSelectUsersDialogButton::getDialogDelegate() const
{
    return m_dialogDelegate;
}

void QnSelectUsersDialogButton::setDialogDelegate(QnResourceSelectionDialogDelegate* delegate)
{
    m_dialogDelegate = delegate;
}

void QnSelectUsersDialogButton::setSubjectValidationPolicy(QnSubjectValidationPolicy* policy)
{
    m_subjectValidation.reset(policy);
}

void QnSelectUsersDialogButton::handleButtonClicked()
{
    // Dialog will be destroyed by delegate editor.
    auto dialog = new SubjectSelectionDialog(this);
    auto ids = getResources();

    // TODO: #vkutin #3.2 Temporary workaround to pass "all users" as a special uuid.
    dialog->setAllUsers(ids.remove(QnBusinessRuleViewModel::kAllUsersId));
    dialog->setCheckedSubjects(ids);

    // TODO: #vkutin Hack till #3.2
    const bool isEmail = dynamic_cast<QnSendEmailActionDelegate*>(m_dialogDelegate) != nullptr;
    dialog->setAllUsersSelectorEnabled(!isEmail);

    if (m_dialogDelegate)
    {
        dialog->setUserValidator(
            [this](const QnUserResourcePtr& user)
            {
                // TODO: #vkutin #3.2 This adapter is rather sub-optimal.
                return m_dialogDelegate->isValid(user->getId());
            });
    }
    else
    {
        dialog->setRoleValidator(
            [this](const QnUuid& roleId)
            {
                return m_subjectValidation
                    ? m_subjectValidation->roleValidity(roleId)
                    : QValidator::Acceptable;
            });

        dialog->setUserValidator(
            [this](const QnUserResourcePtr& user)
            {
                return m_subjectValidation
                    ? m_subjectValidation->userValidity(user)
                    : true;
            });
    }

    const auto updateAlert =
        [this, &dialog]
        {
            // TODO: #vkutin #3.2 Full updates like this are slow. Refactor in 3.2.
            if (m_dialogDelegate)
            {
                dialog->showAlert(m_dialogDelegate->validationMessage(dialog->checkedSubjects()));
            }
            else
            {
                dialog->showAlert(m_subjectValidation
                    ? m_subjectValidation->calculateAlert(dialog->allUsers(),
                        dialog->checkedSubjects())
                    : QString());
            }
        };

    connect(dialog, &SubjectSelectionDialog::changed, this, updateAlert);
    updateAlert();

    if (dialog->exec() != QDialog::Accepted)
        return;

    if (dialog->allUsers())
        setResources(QSet<QnUuid>({QnBusinessRuleViewModel::kAllUsersId}));
    else
        setResources(dialog->checkedSubjects());

    emit commit();
}
