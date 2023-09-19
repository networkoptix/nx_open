// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "build_info_proxy.h"

#include "build_info.h"

namespace nx::build_info {

QmlProxy::QmlProxy(QObject* parent): QObject(parent) {}

QString QmlProxy::vmsVersion()
{
    return nx::build_info::vmsVersion();
}

} // namespace nx::build_info
