// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::mobile::detail {

class BookmarkConstants: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString objectBasedTagName
        READ objectBasedTagName
        CONSTANT)

public:
    BookmarkConstants(QObject* parent = nullptr);

    static QString objectBasedTagName();
};

} // namespace nx::vms::client::mobile::detail
