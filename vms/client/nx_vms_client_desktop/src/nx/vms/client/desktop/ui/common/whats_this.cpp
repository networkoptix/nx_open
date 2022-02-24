// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "whats_this.h"

#include <QtQml/QtQml>
#include <QtWidgets/QWhatsThis>

namespace nx::vms::client::desktop {

bool WhatsThis::inWhatsThisMode()
{
    return QWhatsThis::inWhatsThisMode();
}

void WhatsThis::enterWhatsThisMode()
{
    QWhatsThis::enterWhatsThisMode();
}

void WhatsThis::leaveWhatsThisMode()
{
    QWhatsThis::leaveWhatsThisMode();
}

WhatsThis* WhatsThis::instance()
{
    static WhatsThis instance;
    return &instance;
}

void WhatsThis::registerQmlType()
{
    qmlRegisterSingletonInstance("nx.vms.client.desktop", 1, 0, "WhatsThis", WhatsThis::instance());
}

} // namespace nx::vms::client::desktop
