#include "keywords.h"

namespace nx::vms::rules {

Keywords::Keywords()
{
}

bool Keywords::match(const QVariant& value) const
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
