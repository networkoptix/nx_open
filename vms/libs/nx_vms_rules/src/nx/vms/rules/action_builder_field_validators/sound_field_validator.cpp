// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sound_field_validator.h"

#include "../action_builder_fields/sound_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult SoundFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto soundField = dynamic_cast<const SoundField*>(field);
    if (!NX_ASSERT(soundField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    if (soundField->value().isEmpty())
        return {QValidator::State::Invalid, tr("Sound is not selected")};

    return {};
}

} // namespace nx::vms::rules
