// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtGui/QValidator>

#include <nx/vms/common/system_context.h>

#include "field.h"

namespace nx::vms::rules {

struct ValidationResult
{
    QValidator::State validity = QValidator::State::Acceptable;
    QString description;
    QVariant additionalData; //< Additional data, which can be used in pickers.

    bool isValid() const { return validity == QValidator::State::Acceptable; }

    bool operator==(QValidator::State state) const { return validity == state; };
};

/**
 * This validator is intended for the field validation based on the properties saved in the field's
 * manifest. The structure of the properties is the same for all the fields with the same type,
 * but the values might vary depending on the event or action where the field is used. This
 * properties must contain any type of information like minimum and maximum values, validation
 * policy etc. required for the field validation.
 */
class NX_VMS_RULES_API FieldValidator: public QObject
{
public:
    explicit FieldValidator(QObject* parent = nullptr);

    /** Checks filed validity based on the filed validation policy. */
    virtual ValidationResult validity(
        const Field* field, const Rule* rule, common::SystemContext* context) const = 0;
};

using FieldValidatorPtr = std::unique_ptr<FieldValidator>;

} // namespace nx::vms::rules
