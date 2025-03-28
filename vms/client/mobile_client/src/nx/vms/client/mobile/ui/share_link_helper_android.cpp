// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "share_link_helper.h"

#if defined(Q_OS_ANDROID)

#include <QtCore/QUrl>
#include <QtCore/QJniObject>

namespace nx::vms::client::mobile {

void shareLink(const QUrl& link)
{
    const QJniObject url = QJniObject::fromString(link.toString());
    QJniObject::callStaticMethod<void>(
        "com/nxvms/mobile/utils/QnWindowUtils",
        "shareLink",
        "(Ljava/lang/String;)V",
        url.object<jstring>());
}

} // namespace nx::vms::client::mobile

#endif
