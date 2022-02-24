// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "expected_uuid_field.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

bool ExpectedUuidField::match(const QVariant& value) const
{
    return m_expected == value.value<QnUuid>();
}

QnUuid ExpectedUuidField::expected() const
{
    return m_expected;
}

void ExpectedUuidField::setExpected(const QnUuid& value)
{
    m_expected = value;
}

} // namespace nx::vms::rules
