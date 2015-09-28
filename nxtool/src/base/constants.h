
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
            NoFlags                             = 0x00
            , AllowChangeDateTimeFlag           = 0x01
            , AllowIfConfigFlag                 = 0x02
            , IsFactoryFlag                     = 0x04
            , HasHdd                            = 0x08

            , AllFlags                          = 0xFF
        };
        Q_ENUMS(ServerFlag)
        Q_DECLARE_FLAGS(ServerFlags, ServerFlag)

        enum SystemCommand
        {
            NoCommands                          = 0x00

            , RestartServerCmd                  = 0x01
            , RebootCmd                         = 0x02
            , FactoryDefaultsCmd                = 0x04
            , FactoryDefaultsButNetworkCmd      = 0x08

            , AllCommands                       = 0xFF       
        };
        Q_ENUMS(SystemCommand)
        Q_DECLARE_FLAGS(SystemCommands, SystemCommand)

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



