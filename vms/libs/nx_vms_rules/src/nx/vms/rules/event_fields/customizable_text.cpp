// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "customizable_text.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

CustomizableText::CustomizableText()
{
}

bool CustomizableText::match(const QVariant& value) const
{
    return true;
}

QString CustomizableText::text() const
{
    return m_text;
}

void CustomizableText::setText(const QString& text)
{
    m_text = text;
}

} // namespace nx::vms::rules
