#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {

struct ResourceInfo
{
    ResourceInfo()
    {
        vendor[0] = 0;
        model[0] = 0;
        firmware[0] = 0;
        uid[0] = 0;
        sharedId[0] = 0;
        url[0] = 0;
        login[0] = 0;
        password[0] = 0;
        channel = 0;
    }

    static const int kStringParameterMaxLength = 256;
    static const int kTextParameterMaxLength = 1024;

    char vendor[kStringParameterMaxLength];
    char model[kStringParameterMaxLength];
    char firmware[kStringParameterMaxLength];
    char uid[kStringParameterMaxLength];
    char sharedId[kStringParameterMaxLength];
    char url[kTextParameterMaxLength];
    char login[kStringParameterMaxLength];
    char password[kStringParameterMaxLength];
    int channel;
};

struct Ratio
{
    int numerator;
    int denominator;
};

enum class AttributeType
{
    undefined,
    number,
    boolean,
    string

    // Other more specific types as DateTime, Coordinates, Temperature ???
};

class Attribute //< TODO: #dmishin rename to AbstractAttribute
{
public:
    virtual ~Attribute() {};

    virtual const nx::sdk::AttributeType type() const = 0;
    virtual const char* name() const = 0;
    virtual const char* value()const = 0;
};

enum class Error
{
    noError,
    unknownError, //< TODO: #mike: Consider renaming to "otherError".
    needMoreBufferSpace,
    typeIsNotSupported,
    networkError,
};

static inline const char* toString(Error error)
{
    switch (error)
    {
        case Error::noError: return "noError";
        case Error::unknownError: return "unknownError";
        case Error::needMoreBufferSpace: return "needMoreBufferSpace";
        case Error::typeIsNotSupported: return "typeIsNotSupported";
        case Error::networkError: return "networkError";
        default: return "<unsupported Error>";
    }
}

} // namespace sdk
} // namespace nx
