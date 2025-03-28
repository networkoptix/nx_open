// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "share_link_helper.h"

#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#if not defined(Q_OS_IOS) && not defined(Q_OS_ANDROID)

namespace nx::vms::client::mobile {

void shareLink(const QUrl& link)
{
    qApp->clipboard()->setText(link.toString());
}

} // namespace nx::vms::client::mobile

#endif
