// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "globals.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

namespace nx::vms::client::desktop::jsapi {

void Globals::openUrlExternally(const QString& url)
{
    QDesktopServices::openUrl(url);
}

} // namespace nx::vms::client::desktop::jsapi
