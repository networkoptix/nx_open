#pragma once

#include <boost/optional.hpp>

#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace http {
namespace server {

struct NX_NETWORK_API PasswordLookupResult
{
    enum class Code
    {
        ok,
        notFound,
        otherError,
    };

    enum class DataType
    {
        password,
        ha1
    };

    Code code;
    DataType type;
    nx::String data;

    boost::optional<nx::String> password() const;
    boost::optional<nx::String> ha1() const;
};

class NX_NETWORK_API PlainTextPasswordLookupResultBuilder
{
public:
    static PasswordLookupResult build(nx::String password);
    static PasswordLookupResult build(PasswordLookupResult::Code errorCode);
};

class NX_NETWORK_API Ha1LookupResultBuilder
{
public:
    static PasswordLookupResult build(nx::String ha1);
    static PasswordLookupResult build(PasswordLookupResult::Code errorCode);
};

} // namespace server
} // namespace nx {
} // namespace network {
} // namespace http {
