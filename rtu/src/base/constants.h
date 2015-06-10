
#pragma once

#include <QObject>

namespace rtu
{
    class Constants : public QObject
    {
        Q_OBJECT

    public:
        Constants(QObject *parent = nullptr)
            : QObject(parent) {}
        
        enum ServerFlag
        {
            NoFlags                    = 0x0
            , AllowChangeDateTimeFlag  = 0x1
            , AllowIfConfigFlag        = 0x2
            , IsFactoryFlag            = 0x4
            
            , AllFlags                 = 0xFFFF
        };
        
        Q_ENUMS(ServerFlag)
        Q_DECLARE_FLAGS(ServerFlags, ServerFlag)

        enum Pages
        {
            SettingsPage
            , ProgressPage
            , SummaryPage
        };
        Q_ENUMS(Pages)
    };
}



