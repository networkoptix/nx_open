// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_constants.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {
namespace bookmarks {

BookmarkConstants::BookmarkConstants(QObject* parent):
    QObject(parent)
{
}

QString BookmarkConstants::objectBasedTagName()
{
    return "object-base";
}

void BookmarkConstants::registerQmlType()
{
    qmlRegisterSingletonType<BookmarkConstants>(
        "nx.vms.client.core", 1, 0, "BookmarkConstants",
        [](QQmlEngine*, QJSEngine*) -> QObject*
        {
            return new BookmarkConstants();
        });
}

} // namespace bookmarks
} // namespace nx::vms::client::core
