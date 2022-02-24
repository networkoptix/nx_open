// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_filter_setup.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

TextFilterSetup::TextFilterSetup(QObject* parent):
    QObject(parent)
{
}

QString TextFilterSetup::text() const
{
    return m_text;
}

void TextFilterSetup::setText(const QString& value)
{
    const auto newText = value.trimmed();
    if (m_text == newText)
        return;

    m_text = newText;
    NX_VERBOSE(this, "Set filter text to \"%1\"", m_text);
    emit textChanged();
}

} // namespace nx::vms::client::desktop
