// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "navigation_bar_utils.h"

#include <QtCore/QtGlobal>
#include <QtQml/QtQml>

namespace nx::vms::client::mobile {

void NavigationBarUtils::registerQmlType()
{
    qmlRegisterUncreatableType<NavigationBarUtils>("Nx.Mobile", 1, 0, "NavigationBar",
        "Cannot create an instance of NavigationBarUtils");
}

#if !defined(Q_OS_ANDROID)

NavigationBarUtils::Type NavigationBarUtils::navigationBarType()
{
    return Type::NoNavigationBar;
};

int NavigationBarUtils::barSize()
{
    return 0;
}

#endif

} // namespace nx::vms::client::mobile
