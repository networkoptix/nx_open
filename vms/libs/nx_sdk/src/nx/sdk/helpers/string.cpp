#include "string.h"

namespace nx {
namespace sdk {

String::String(std::string s):
    m_string(std::move(s))
{
}

String::String(const char* s)
{
    setString(s);
}

const char* String::str() const
{
    return m_string.c_str();
}

void String::setString(std::string s)
{
    m_string = std::move(s);
}

void String::setString(const char* s)
{
    if (s)
        m_string = s;
    else
        m_string.clear();
}

int String::size() const
{
    return static_cast<int>(m_string.size());
}

bool String::empty() const
{
    return m_string.empty();
}

} // namespace sdk
} // namespace nx
