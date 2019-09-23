#pragma once

#if defined(_WIN32)
#include <filesystem>
#endif

#include <string>

namespace nx::utils::filesystem {

#if defined(_WIN32)

using namespace std::experimental::filesystem;

#else

#define NX_STD_FILESYSTEM_IMPLEMENTATION

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

#endif

} // namespace nx::utils::filesystem
