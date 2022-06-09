// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_text_field.h"

#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>

namespace nx::vms::rules {

CustomizableTextField::CustomizableTextField()
{
}

bool CustomizableTextField::match(const QVariant& value) const
{
    // Field value used for event customization, matches any event value.
    return true;
}

QString CustomizableTextField::text() const
{
    return m_text;
}

void CustomizableTextField::setText(const QString& text)
{
    m_text = text;
}

} // namespace nx::vms::rules
