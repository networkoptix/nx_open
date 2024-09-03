// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "branding_proxy.h"

#include "branding.h"

namespace nx::branding {

QmlProxy::QmlProxy(QObject* parent): QObject(parent) {}

QString QmlProxy::cloudName()
{
    return nx::branding::cloudName();
}

QString QmlProxy::company()
{
    return nx::branding::company();
}

QString QmlProxy::companyUrl()
{
    return nx::branding::companyUrl();
}

QString QmlProxy::vmsName()
{
    return nx::branding::vmsName();
}

bool QmlProxy::isDesktopClientCustomized()
{
    return nx::branding::isDesktopClientCustomized();
}

QString QmlProxy::supportAddress()
{
    return nx::branding::supportAddress();
}

QString QmlProxy::defaultLocale()
{
    return nx::branding::defaultLocale();
}

} // namespace nx::branding
