#pragma once

#include <string>

namespace nx::utils::filesystem {

class NX_UTILS_API path
{
public:
    path() = default;
    path(const std::string& str);

    path filename() const;
    bool has_parent_path() const;
    path parent_path() const;

    std::string string() const;

    bool operator==(const path& other) const;
    bool operator!=(const path& other) const;

private:
    std::string m_pathStr;
};

} // namespace nx::utils::filesystem
