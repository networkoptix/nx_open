
#pragma once

#include <QObject>

namespace rtu {

class Constants : public QObject
{
    Q_OBJECT

public:
    Constants(QObject *parent = nullptr)
        : QObject(parent) {}
        
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
};

} // namespace rtu
