// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "navigation_bar_utils.h"

#include <QtCore/QJniObject>
#include <QtGui/QGuiApplication>

namespace {

const char* kUtilsClass = "com/nxvms/mobile/utils/QnWindowUtils";

} // namespace

namespace nx::vms::client::mobile {

NavigationBarUtils::Type NavigationBarUtils::navigationBarType()
{
    return static_cast<NavigationBarUtils::Type>(
        QJniObject::callStaticMethod<jint>(
            kUtilsClass,
            "navigationBarType",
            "(IIII)I",
            NavigationBarUtils::NoNavigationBar,
            NavigationBarUtils::BottomNavigationBar,
            NavigationBarUtils::LeftNavigationBar,
            NavigationBarUtils::RightNavigationBar));
};

int NavigationBarUtils::barSize()
{
    const int size = QJniObject::callStaticMethod<jint>(kUtilsClass, "getNavigationBarSize");
    return static_cast<int>(size / qApp->devicePixelRatio());
};

} // namespace nx::vms::client::mobile
