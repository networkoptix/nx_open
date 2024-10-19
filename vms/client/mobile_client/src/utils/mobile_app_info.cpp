// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_app_info.h"

#include <nx/utils/software_version.h>
#include <nx/utils/log/assert.h>

#include <nx/build_info.h>
#include <nx/branding.h>

QnMobileAppInfo::QnMobileAppInfo(QObject* parent)
    : QObject(parent)
{

}

QString QnMobileAppInfo::productName() const
{
    return nx::branding::vmsName();
}

QString QnMobileAppInfo::version() const
{
    return QString("%1.%2 (rev: %3)").arg(
        nx::build_info::mobileClientVersion(),
        QString::number(nx::build_info::buildNumber()),
        nx::build_info::revision());
}

QString QnMobileAppInfo::cloudName() const
{
    return nx::branding::cloudName();
}

QString QnMobileAppInfo::shortCloudName() const
{
    return nx::branding::shortCloudName();
}
