// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtGui/QScreen>
#include <QtWidgets/QWidget>

#include <core/misc/screen_snap.h>

QRect screenSnapsGeometry(
    const QnScreenSnaps& snaps,
    const QList<QRect>& screenGeometries);

QRect screenSnapsGeometryScreenRelative(
    const QnScreenSnaps& snaps,
    const QList<QRect>& physicalScreenGeometries,
    QScreen* screen);

QRect screenSnapsToWidgetGeometry(const QnScreenSnaps& snaps, QWidget* widget);
