// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_server_field_validator.h"

#include <nx/vms/rules/server_validation_policy.h>

#include "../event_filter_fields/source_server_field.h"
#include "../manifest.h"
#include "../utils/resource.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult SourceServerFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto sourceServerField = dynamic_cast<const SourceServerField*>(field);
    if (!NX_ASSERT(sourceServerField))
        return {QValidator::State::Invalid, {tr("Invalid field type is provided")}};

    const auto serversSelection = sourceServerField->selection();
    const auto sourceServerFieldProperties = sourceServerField->properties();

    if (!serversSelection.isEmpty() && !sourceServerFieldProperties.validationPolicy.isEmpty())
    {
        QnMediaServerResourceList servers = utils::servers(serversSelection, context);

        QValidator::State serversValidity{QValidator::State::Acceptable};
        if (sourceServerFieldProperties.validationPolicy == kHasFanMonitoringValidationPolicy)
            serversValidity = utils::serversValidity<QnFanErrorPolicy>(servers);
        else if (sourceServerFieldProperties.validationPolicy == kHasPoeManagementValidationPolicy)
            serversValidity = utils::serversValidity<QnPoeOverBudgetPolicy>(servers);
        else
            return {QValidator::State::Invalid, tr("Unexpected validation policy")};

        if (serversValidity == QValidator::State::Acceptable)
            return {};

        return {
            serversValidity,
            serversValidity == QValidator::State::Intermediate
                ? tr("Not all servers are suitable")
                : tr("There are no suitable servers")};
    }

    if (!sourceServerFieldProperties.allowEmptySelection && serversSelection.isEmpty())
        return {QValidator::State::Invalid, tr("Select at least one server")};

    return {};
}

} // namespace nx::vms::rules
