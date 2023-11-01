// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "subject_target_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QPushButton>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

QnSubjectTargetActionWidget::QnSubjectTargetActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    m_helper(new vms::event::StringsHelper(systemContext))
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

    dialog.setRoleValidator(
        [this](const QnUuid& roleId) { return roleValidity(roleId); });

    dialog.setUserValidator(
        [this](const QnUserResourcePtr& user) { return userValidity(user); });

    if (m_options)
        dialog.setOptions(m_options.value());

    QSet<QnUuid> selected;
    for (auto id: params.additionalResources)
        selected << id;

    dialog.setCheckedSubjects(selected);
    dialog.setAllUsers(params.allUsers);

    const auto updateAlert =
        [this, &dialog]()
        {
            dialog.showAlert(calculateAlert(dialog.allUsers(), dialog.checkedSubjects()));
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
            {
                NX_ASSERT(!id.isNull());
                params.additionalResources.push_back(id);
            }
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

    const auto icon = [](const QString& path) -> QIcon
    {
        static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions colorSubs = {
            {QnIcon::Normal, {.primary = "light10"}}, {QnIcon::Selected, {.primary = "light4"}}};
        return qnSkin->icon(path, colorSubs);
    };

    const auto params = model()->actionParams();
    if (params.allUsers)
    {
        m_subjectsButton->setText(vms::event::StringsHelper::allUsersText());

        const auto users = resourcePool()->getResources<QnUserResource>()
            .filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

        const bool allValid =
            std::all_of(users.begin(), users.end(),
                [this](const QnUserResourcePtr& user) { return userValidity(user); });

        m_subjectsButton->setIcon(icon(allValid
            ? lit("tree/users.svg")
            : lit("tree/users_alert.svg")));
    }
    else
    {
        QnUserResourceList users;
        UserGroupDataList groups;
        nx::vms::common::getUsersAndGroups(systemContext(),
            params.additionalResources,
            users,
            groups);

        users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

        if (users.empty() && groups.empty())
        {
            m_subjectsButton->setText(vms::event::StringsHelper::needToSelectUserText());
            m_subjectsButton->setIcon(icon(lit("tree/user_alert.svg")));
        }
        else
        {
            const bool allValid =
                std::all_of(users.begin(), users.end(),
                    [this](const QnUserResourcePtr& user) { return userValidity(user); })
                && std::all_of(groups.begin(), groups.end(),
                    [this](const UserGroupData& group)
                    {
                        return roleValidity(group.id) == QValidator::Acceptable;
                    });

            // TODO: #vkutin #3.2 Color the button red if selection is completely invalid.

            const bool multiple = users.size() > 1 || !groups.empty();
            m_subjectsButton->setText(m_helper->actionSubjects(users, groups));
            m_subjectsButton->setIcon(icon(multiple
                ? (allValid ? lit("tree/users.svg") : lit("tree/users_alert.svg"))
                : (allValid ? lit("tree/user.svg") : lit("tree/user_alert.svg"))));
        }
    }
}

void QnSubjectTargetActionWidget::setValidationPolicy(QnSubjectValidationPolicy* policy)
{
    m_validationPolicy.reset(policy);
}

void QnSubjectTargetActionWidget::setDialogOptions(const CustomizableOptions& options)
{
    m_options.emplace(options);
}

QValidator::State QnSubjectTargetActionWidget::roleValidity(const QnUuid& roleId) const
{
    return m_validationPolicy ? m_validationPolicy->roleValidity(roleId) : QValidator::Acceptable;
}

bool QnSubjectTargetActionWidget::userValidity(const QnUserResourcePtr& user) const
{
    return m_validationPolicy ? m_validationPolicy->userValidity(user) : true;
}

QString QnSubjectTargetActionWidget::calculateAlert(
    bool allUsers, const QSet<QnUuid>& subjects) const
{
    return m_validationPolicy
        ? m_validationPolicy->calculateAlert(allUsers, subjects)
        : QString();
}
