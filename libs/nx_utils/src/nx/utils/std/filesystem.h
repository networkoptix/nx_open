#pragma once

#if defined(_WIN32)
#include <filesystem>

namespace std { namespace filesystem { using namespace std::experimental::filesystem; } }
#else

#define NX_STD_FILESYSTEM_IMPLEMENTATION

#include <string>

namespace std {
namespace filesystem {
namespace old_version_support {

class NX_UTILS_API path
{
public:
    using string_type = std::string;

    path() = default;
    path(const char* str);
    path(const std::string& str);

    path filename() const;
    bool has_filename() const;

    bool has_parent_path() const;
    path parent_path() const;

    std::string string() const;

    bool operator==(const path& other) const;
    bool operator!=(const path& other) const;
    bool empty() const;

    const string_type& native() const;

private:
    std::string m_pathStr;
};

inline bool operator==(const path& one, const path& two)
{
    return one.native() == two.native();
}

inline bool operator!=(const path& one, const path& two)
{
    return one.native() != two.native();
}

} // namespace old_version_support
} // namespace filesystem
} // namespace std

#endif
