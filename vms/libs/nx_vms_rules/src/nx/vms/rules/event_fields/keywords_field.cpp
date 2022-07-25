// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "keywords_field.h"

#include <QtCore/QVariant>

#include <nx/utils/string.h>

namespace nx::vms::rules {

bool KeywordsField::match(const QVariant& value) const
{
    if (m_list.empty())
        return true;

    const auto& string = value.toString();

    if (string.isEmpty())
        return false;

    return std::any_of(m_list.cbegin(), m_list.cend(),
        [&string](const auto& word)
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
        m_list.clear();
        for (const auto& keyword: nx::utils::smartSplit(m_string, ' ', Qt::SkipEmptyParts))
        {
            auto trimmed = nx::utils::trimAndUnquote(keyword);
            if (!trimmed.isEmpty())
                m_list.append(std::move(trimmed));
        }

        emit stringChanged();
    }
}

} // namespace nx::vms::rules
