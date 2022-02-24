// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>

#include <nx/utils/uuid.h>
#include <core/misc/screen_snap.h>

/**
* Defines types for "raw" video wall model.
*/

namespace nx::vms::client::desktop {
namespace videowall {

typedef QRect ScreenGeometry;
typedef QVector<ScreenGeometry> Screens;

struct Item
{
    QnUuid id;
    QString name;
    QnScreenSnaps snaps;
    bool isAdded = false;
};
typedef QVector<Item> Items;

struct Model
{
    Screens screens;
    Items items;
};

} // namespace videowall
} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::videowall::ScreenGeometry)
Q_DECLARE_METATYPE(nx::vms::client::desktop::videowall::Screens)

Q_DECLARE_METATYPE(nx::vms::client::desktop::videowall::Item)
Q_DECLARE_METATYPE(nx::vms::client::desktop::videowall::Items)

Q_DECLARE_METATYPE(nx::vms::client::desktop::videowall::Model)
