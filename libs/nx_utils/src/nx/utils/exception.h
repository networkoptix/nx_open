#pragma once

#include <exception>
#include <optional>

#include <QString>

namespace nx::utils {

class NX_UTILS_API Exception: public std::exception
{
public:
    virtual QString message() const = 0;
    virtual const char* what() const noexcept override final;

private:
    mutable std::optional<std::string> m_whatCache;
};

} // namespace nx::utils

