// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../field_validator.h"

namespace nx::vms::rules {

class SourceServerFieldValidator: public FieldValidator
{
    Q_OBJECT

public:
    using FieldValidator::FieldValidator;

    ValidationResult validity(
        const Field* field, const Rule* rule, common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
