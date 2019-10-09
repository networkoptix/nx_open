#include "filesystem.h"

namespace nx::utils::filesystem {

path::path(const std::string& str):
    m_pathStr(str)
{
}

path::path(const char* str):
    m_pathStr(str)
{
}

path path::filename() const
{
    const auto separatorPos = m_pathStr.find_last_of("\\/");
    auto result = separatorPos != std::string::npos
        ? m_pathStr.substr(separatorPos+1)
        : m_pathStr;
    return result;
}

bool path::has_filename() const
{
    return !filename().empty();
}

bool path::has_parent_path() const
{
    return m_pathStr.find_first_of("\\/") != std::string::npos;
}

path path::parent_path() const
{
    auto separatorPos = m_pathStr.find_last_of("\\/");
    if (separatorPos == std::string::npos)
        return path();

    // Conforming to msvc std::filesystem implementation:
    // If parent path is actually a mswin drive (e.g., c:\), then path separator is added.
    if (separatorPos > 1 && m_pathStr[separatorPos - 1] == ':')
        ++separatorPos;

    return m_pathStr.substr(0, separatorPos);
}

path::string_type path::string() const
{
    return m_pathStr;
}

bool path::empty() const
{
    return m_pathStr.empty();
}

const path::string_type& path::native() const
{
    return m_pathStr;
}

} // namespace nx::utils::filesystem
