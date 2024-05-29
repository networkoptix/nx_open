// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_server_field_validator.h"

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/server_validation_policy.h>

#include "../action_builder_fields/target_server_field.h"
#include "../manifest.h"
#include "../strings.h"
#include "../utils/resource.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetServerFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto targetServerField = dynamic_cast<const TargetServerField*>(field);
    if (!NX_ASSERT(targetServerField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto serversSelection = targetServerField->selection();
    const auto targetServerFieldProperties = targetServerField->properties();

    if (!serversSelection.isEmpty() && !targetServerFieldProperties.validationPolicy.isEmpty())
    {
        if (targetServerFieldProperties.validationPolicy == kHasBuzzerValidationPolicy)
        {
            QnMediaServerResourceList servers = utils::servers(serversSelection, context);

            const auto serversValidity = utils::serversValidity<QnBuzzerPolicy>(servers);

            if (serversValidity == QValidator::State::Acceptable)
                return {};

            return {serversValidity, Strings::noSuitableServers(serversValidity)};
        }

        return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
    }

    if (!targetServerFieldProperties.allowEmptySelection && serversSelection.isEmpty())
        return {QValidator::State::Invalid, Strings::selectServer()};

    return {};
}

} // namespace nx::vms::rules