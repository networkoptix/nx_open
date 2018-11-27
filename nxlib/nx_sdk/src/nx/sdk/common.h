#pragma once

namespace nx {
namespace sdk {

struct DeviceInfo
{
    DeviceInfo()
    {
        vendor[0] = 0;
        model[0] = 0;
        firmware[0] = 0;
        uid[0] = 0;
        sharedId[0] = 0;
        url[0] = 0;
        login[0] = 0;
        password[0] = 0;
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
    int channel = 0;
    int logicalId = 0;
};

struct CameraInfo: DeviceInfo
{
    // Currently, CameraInfo has no specific fields.
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
    string,
    // TODO: Consider adding other specific types like DateTime, Coordinates, Temperature.
};

class IAttribute
{
public:
    virtual ~IAttribute() = default;

    virtual AttributeType type() const = 0;
    virtual const char* name() const = 0;
    virtual const char* value() const = 0;
};

enum class Error
{
    noError,
    unknownError, //< TODO: Consider renaming to "genericError".
    needMoreBufferSpace,
    typeIsNotSupported,
    networkError,
};

class IStringList
{
public:
    virtual ~IStringList() = default;
    virtual int count() const = 0;
    virtual const char* at(int index) const = 0;
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
