#include "common_string_list.h"

namespace nx {
namespace sdk {

int CommonStringList::count() const
{
    return m_strings.size();
}

const char* CommonStringList::at(int index) const
{
    if (index < 0 || index >= (int) m_strings.size())
        return nullptr;

    return m_strings[index].c_str();
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
