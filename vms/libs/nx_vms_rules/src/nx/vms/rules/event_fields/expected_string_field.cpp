// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "expected_string_field.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

bool ExpectedStringField::match(const QVariant& value) const
{
    return m_expected == value;
}

QString ExpectedStringField::expected() const
{
    return m_expected;
}

void ExpectedStringField::setExpected(const QString& value)
{
    m_expected = value;
}

} // namespace nx::vms::rules
