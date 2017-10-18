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

struct Attribute
{
    AttributeType type = AttributeType::undefined;
    char* name = nullptr;
    char* value = nullptr;
};

enum class Error
{
    noError,
    unknownError,
    needMoreBufferSpace,
    typeIsNotSupported,
    networkError
};

} // namespace sdk
} // namespace nx
