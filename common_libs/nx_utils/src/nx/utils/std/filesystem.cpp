#include "filesystem.h"

#if defined(_WIN32) && 0
#else

namespace std {
namespace filesystem {

path::path(const std::string& str):
    m_pathStr(str)
{
}

path path::filename() const
{
    const auto separatorPos = m_pathStr.find_last_of("\\/");
    return separatorPos != std::string::npos
        ? m_pathStr.substr(separatorPos+1)
        : *this;
}

bool path::has_parent_path() const
{
    return m_pathStr.find_first_of("\\/") != std::string::npos;
}

path path::parent_path() const
{
    const auto separatorPos = m_pathStr.find_last_of("\\/");
    return separatorPos != std::string::npos
        ? m_pathStr.substr(0, separatorPos)
        : path();
}

std::string path::string() const
{
    return m_pathStr;
}

bool path::operator==(const path& other) const
{
    return m_pathStr == other.m_pathStr;
}

bool path::operator!=(const path& other) const
{
    return m_pathStr != other.m_pathStr;
}

} // namespace filesystem
} // namespace std

#endif
