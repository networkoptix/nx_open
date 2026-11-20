// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "globals.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop::jsapi {

namespace {

bool isAllowedExternalUrl(const QUrl& url)
{
    if (!url.isValid())
        return false;

    if (nx::network::http::isUrlScheme(url.scheme()))
        return !url.host().isEmpty();

    return url.scheme() == "mailto" || url.scheme() == "tel";
}

} // namespace

void Globals::openUrlExternally(const QString& url)
{
    const QUrl parsedUrl(url);
    if (!isAllowedExternalUrl(parsedUrl))
    {
        NX_WARNING(this, "Refused to open external URL: %1", url);
        return;
    }

    QDesktopServices::openUrl(parsedUrl);
}

} // namespace nx::vms::client::desktop::jsapi
