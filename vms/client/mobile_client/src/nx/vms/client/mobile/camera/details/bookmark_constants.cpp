// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_constants.h"

namespace nx::vms::client::mobile::detail {

BookmarkConstants::BookmarkConstants(QObject* parent):
    QObject(parent)
{
}

QString BookmarkConstants::objectBasedTagName()
{
    return "object-base";
}

} // namespace nx::vms::client::mobile::detail
