// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_list.h"

namespace nx::sdk {

int StringList::count() const
{
    return (int) m_strings.size();
}

const char* StringList::at(int index) const
{
    if (index < 0 || index >= (int) m_strings.size())
        return nullptr;

    return m_strings[index].c_str();
}

void StringList::addString(std::string str)
{
    m_strings.push_back(std::move(str));
}

void StringList::clear()
{
    m_strings.clear();
}

} // namespace nx::sdk
