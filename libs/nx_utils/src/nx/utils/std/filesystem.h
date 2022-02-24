// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::utils::filesystem {

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

    path extension() const;
    bool has_extension() const;

    path stem() const;
    bool has_stem() const;

    string_type string() const;

    bool empty() const;

    const string_type& native() const;

private:
    string_type m_pathStr;
};

inline bool operator==(const path& one, const path& two)
{
    return one.native() == two.native();
}

inline bool operator!=(const path& one, const path& two)
{
    return one.native() != two.native();
}

} // namespace nx::utils::filesystem
