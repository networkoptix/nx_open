#include "common_string_list.h"

namespace nx {
namespace sdk {

int CommonStringList::count() const
{
    return static_cast<int>(m_strings.size());
}

const char* CommonStringList::at(int index) const
{
    auto i = static_cast<decltype(m_strings)::size_type>(index);
    if (i >= m_strings.size())
        return nullptr;

    return m_strings.at(i).c_str();
}

void CommonStringList::addString(std::string str)
{
    m_strings.push_back(std::move(str));
}

void CommonStringList::clear()
{
    m_strings.clear();
}

} // namespace sdk
} // namespace nx
