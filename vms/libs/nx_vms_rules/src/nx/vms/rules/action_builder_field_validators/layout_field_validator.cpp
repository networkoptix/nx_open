// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_field_validator.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/resource.h>

#include "../rule.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult LayoutFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto layoutField = dynamic_cast<const LayoutField*>(field);
    if (!NX_ASSERT(layoutField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    if (layoutField->value().isNull())
        return {QValidator::State::Invalid, tr("Select layout")};

    auto targetUserField =
        rule->actionBuilders().front()->fieldByName<TargetUserField>(utils::kUsersFieldName);
    if (targetUserField)
    {
        const auto users = utils::users(targetUserField->selection(), context);
        if (users.empty())
            return {};

        const auto layout =
            context->resourcePool()->getResourceById<QnLayoutResource>(layoutField->value());

        if (!layout->isShared())
        {
            if (users.size() == 1 && layout->getParentId() == (*users.cbegin())->getId())
                return {};

            const auto layoutOwner =
                context->resourcePool()->getResourceById<QnUserResource>(layout->getParentId());
            return {
                QValidator::State::Invalid,
                tr("Chosen local layout can only be shown to its owner %1").arg(layoutOwner->getName())};
        }

        const auto layoutParent = layout->getParentResource();
        const auto accessManager = context->resourceAccessManager();
        int usersWithoutAccessCount{0};
        for (const auto& user: users)
        {
            if (!accessManager->hasPermission(user, layout, Qn::ReadPermission))
                ++usersWithoutAccessCount;
        }

        if (usersWithoutAccessCount == 0)
            return{};

        return {
            QValidator::State::Intermediate,
            usersWithoutAccessCount == users.size()
                ? tr("None of selected users have access to the selected layout")
                : tr("Some users do not have access to the selected layout")};
    }

    return {};
}

} // namespace nx::vms::rules
