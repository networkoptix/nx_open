#pragma once

namespace nx {
namespace sdk {

class IAttribute
{
public:
    enum class Type
    {
        undefined,
        number,
        boolean,
        string,
        // TODO: Consider adding other specific types like DateTime, Coordinates, Temperature.
    };

    virtual ~IAttribute() = default;

    virtual Type type() const = 0;
    virtual const char* name() const = 0;
    virtual const char* value() const = 0;
    virtual float confidence() const = 0;
};

} // namespace sdk
} // namespace nx
