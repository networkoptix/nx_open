
#pragma once

#include <QObject>

namespace rtu
{
    class Constants : public QObject
    {
    public:
        Constants(QObject *parent = nullptr)
            : QObject(parent) {}
        
        enum class ServerFlag
        {
            kNoFlags                            = 0x0
            , kAllowChangeDateTime              = 0x1
            , kAllowChangeInterfaceSettings     = 0x2
            , kIsFactory                        = 0x4
            
            , kAllFlags                         = 0xFFFF
        };
        
        Q_ENUMS(ServerFlag)
        Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
    };
}


