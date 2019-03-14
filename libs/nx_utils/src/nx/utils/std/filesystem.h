#pragma once

#if defined(_WIN32)
#include <filesystem>

namespace std { namespace filesystem { using namespace std::experimental::filesystem; } }
#else

#define NX_STD_FILESYSTEM_IMPLEMENTATION

#include <string>

namespace std {
namespace filesystem {

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

} // namespace filesystem
} // namespace std

#endif
