// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "back_gesture_workaround.h"

#if !defined(Q_OS_ANDROID)

namespace nx::vms::client::mobile {

struct BackGestureWorkaround::Private {};

BackGestureWorkaround::BackGestureWorkaround(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

BackGestureWorkaround::~BackGestureWorkaround()
{
}

void BackGestureWorkaround::setWorkaroundParentItem(QQuickItem* /*item*/)
{
}

QQuickItem* BackGestureWorkaround::workaroundParentItem() const
{
    return nullptr;
}

} // namespace nx::vms::client::mobile

#endif
