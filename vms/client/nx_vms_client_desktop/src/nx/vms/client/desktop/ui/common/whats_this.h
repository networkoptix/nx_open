// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

class WhatsThis: public QObject
{
    Q_OBJECT
    WhatsThis() = default;

public:
    Q_INVOKABLE static bool inWhatsThisMode();
    Q_INVOKABLE static void enterWhatsThisMode();
    Q_INVOKABLE static void leaveWhatsThisMode();

    static WhatsThis* instance();
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
