// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::mobile {

class NavigationBarUtils
{
    Q_GADGET

public:
    static void registerQmlType();

    NavigationBarUtils() = delete;

    enum Type
    {
        NoNavigationBar,
        BottomNavigationBar,
        LeftNavigationBar,
        RightNavigationBar
    };
    Q_ENUM(Type);

    static Type navigationBarType();

    static int barSize();
};

} // namespace nx::vms::client::mobile
