// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "share_link_helper.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

namespace nx::vms::client::mobile {

void shareLink(const QString& link)
{
    qApp->clipboard()->setText(link);
}

} // namespace nx::vms::client::mobile
