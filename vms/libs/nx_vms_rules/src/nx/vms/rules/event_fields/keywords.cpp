#include "keywords.h"

namespace nx::vms::rules {

Keywords::Keywords()
{
}

bool Keywords::match(const QVariant& value) const
{
    const auto& string = value.toString();
    for (const auto& word: m_list)
    {
        if (string.contains(word))
            return true;
    }

    return false;
}

QString Keywords::string() const
{
    return m_string;
}

void Keywords::setString(const QString& string)
{
    m_string = string;
    m_list = string.split(' ', QString::SkipEmptyParts);
}

} // namespace nx::vms::rules
