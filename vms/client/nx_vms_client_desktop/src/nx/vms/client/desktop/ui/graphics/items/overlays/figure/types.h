// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointF>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop::figure {

enum class FigureType
{
    invalid,
    dummy,
    polyline,
    box,
    polygon,
    point
};

using EngineId = nx::Uuid;

class Figure;
using FigureId = QString;
using FigurePtr = QSharedPointer<Figure>;

struct FigureInfo
{
    FigurePtr figure;
    QString labelText;
};

using FigureInfoHash = QHash<FigureId, FigureInfo>;
using FigureInfosByEngine = QHash<EngineId, FigureInfoHash>;

class RoiFiguresWatcher;
using FiguresWatcherPtr = QSharedPointer<RoiFiguresWatcher>;

using Points = QVector<QPointF>;
using Lines = QVector<QLineF>;

class Renderer;
using RendererPtr = QSharedPointer<Renderer>;

} // namespace nx::vms::client::desktop::figure
