// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "keywords_field.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

bool KeywordsField::match(const QVariant& value) const
{
    const auto& string = value.toString();

    if (string.isEmpty())
        return false;

    return std::any_of(m_list.cbegin(), m_list.cend(),
        [string](const auto& word)
        {
            return string.contains(word);
        });
}

QString KeywordsField::string() const
{
    return m_string;
}

void KeywordsField::setString(const QString& string)
{
    if (m_string != string)
    {
        m_string = string;
        m_list = string.split(' ', QString::SkipEmptyParts);

        emit stringChanged();
    }
}

} // namespace nx::vms::rules
