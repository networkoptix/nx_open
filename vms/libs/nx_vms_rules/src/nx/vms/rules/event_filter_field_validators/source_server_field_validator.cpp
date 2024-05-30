// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_server_field_validator.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/server_validation_policy.h>

#include "../event_filter_fields/source_server_field.h"
#include "../manifest.h"
#include "../strings.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult SourceServerFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto sourceServerField = dynamic_cast<const SourceServerField*>(field);
    if (!NX_ASSERT(sourceServerField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto sourceServerFieldProperties = sourceServerField->properties();
    const bool isValidSelection =
        !sourceServerField->ids().empty() || sourceServerFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, Strings::selectServer()};

    if (!sourceServerFieldProperties.validationPolicy.isEmpty())
    {
        QnMediaServerResourceList servers =
            context->resourcePool()->getResourcesByIds<QnMediaServerResource>(sourceServerField->ids());

        QValidator::State serversValidity{QValidator::State::Acceptable};
        if (sourceServerFieldProperties.validationPolicy == kHasFanMonitoringValidationPolicy)
            serversValidity = utils::serversValidity<QnFanErrorPolicy>(servers);
        else if (sourceServerFieldProperties.validationPolicy == kHasPoeManagementValidationPolicy)
            serversValidity = utils::serversValidity<QnPoeOverBudgetPolicy>(servers);
        else
            return {QValidator::State::Invalid, Strings::unexpectedPolicy()};

        if (serversValidity == QValidator::State::Acceptable)
            return {};

        return {serversValidity, Strings::noSuitableServers(serversValidity)};
    }

    return {};
}

} // namespace nx::vms::rules
