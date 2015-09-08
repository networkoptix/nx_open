
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
            ,  EmptyPage
        };
        Q_ENUMS(Pages)
        
        enum LoggedState
        {
            LoggedToAllServers
            , PartiallyLogged
            , NotLogged
        };
        Q_ENUMS(LoggedState)

        enum ItemType
        {
            SystemItemType
            , ServerItemType
            , UnknownGroupType
            , UnknownEntityType
        };
        Q_ENUMS(ItemType)

        enum AffectedEntity
        {
            kNoEntitiesAffected         = 0x0
            , kPortAffected             = 0x1
            , kPasswordAffected         = 0x2
            , kSystemNameAffected       = 0x4

            , kIpAddressAffected        = 0x10
            , kMaskAffected             = 0x20
            , kDHCPUsageAffected        = 0x30
            , kDNSAffected              = 0x40
            , kGatewayAffected          = 0x80
            , kAllAddressFlagsAffected  = kIpAddressAffected | kMaskAffected | kDHCPUsageAffected
                | kDNSAffected | kGatewayAffected

            , kDateTimeAffected         = 0x100
            , kTimeZoneAffected         = 0x200
        
            , kAllEntitiesAffected      = 0xFFF
        };
        Q_DECLARE_FLAGS(AffectedEntities, AffectedEntity)
    };
    Q_DECLARE_OPERATORS_FOR_FLAGS(Constants::AffectedEntities)
}



