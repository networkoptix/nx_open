#pragma once

class Ptz: public QObject
{
    Q_OBJECT

public:
    enum class Capability
    {
        Empty       = 0x0,

        AutoFocus   = 0x1,
        FourWay     = 0x2,
        EightWay    = 0x4
    };
    Q_ENUM(Capability)
    Q_DECLARE_FLAGS(Capabilities, Capability)
};

Q_DECLARE_METATYPE(Ptz::Capability)
Q_DECLARE_METATYPE(Ptz::Capabilities)

