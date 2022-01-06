// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grid_viewport.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/QQmlInfo>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/utils/geometry.h>

namespace nx::vms::client::core {

GridViewportAttached::GridViewportAttached(QObject* parent):
    QObject(parent)
{
}

QRect GridViewportAttached::geometry() const
{
    return m_geometry;
}

void GridViewportAttached::setGeometry(const QRect& geometry)
{
    if (m_geometry == geometry)
        return;

    m_geometry = geometry;
    emit geometryChanged();
}

QQuickItem* GridViewportAttached::item()
{
    return qobject_cast<QQuickItem*>(parent());
}

GridViewport* GridViewportAttached::parentGrid()
{
    auto parentItem = item();

    if (!parentItem)
    {
        NX_WARNING(this, "Grid must be attached to Item elements");
        return nullptr;
    }

    return qobject_cast<GridViewport*>(parentItem->parentItem());
}

GridViewportParameters::GridViewportParameters(const QRect& bounds, qreal scale):
    bounds(bounds),
    center(QRectF(bounds).center()),
    scale(scale)
{
}

bool GridViewportParameters::operator==(const GridViewportParameters& other) const
{
    return bounds == other.bounds && qFuzzyCompare(scale, other.scale);
}

class GridViewport::Private
{
    GridViewport* q;

public:
    qreal cellAspectRatio = 16.0 / 9.0;
    qreal scale = 1.0;
    QPointF center;

    GridViewportParameters fitContentParameters;
    GridViewportParameters fitZommedItemParameters;
    qreal oneCellScale = 1.0;

    QQuickItem* zoomedItem = nullptr;

    QHash<QQuickItem*, QRect> geometryByItem;

public:
    Private(GridViewport* parent):
        q(parent)
    {
    }

    void positionItems()
    {
        const QPointF visualCenter(q->width() / 2, q->height() / 2);

        for (auto it = geometryByItem.begin(); it != geometryByItem.end(); ++it)
        {
            const auto item = it.key();
            const auto& geomtery = it.value();
            const auto itemGeometry = q->mapToViewport(geomtery);

            item->setPosition(itemGeometry.topLeft());
            item->setSize(itemGeometry.size());
        }
    }

    void updateItemGridGeometry(QQuickItem* item, const QRect& geometry)
    {
        geometryByItem[item] = geometry;
        updateFitContentsParameters();
        positionItems();
        if (item == zoomedItem)
            updateZoomedItemParameters();
    }

    void updateFitContentsParameters(bool recalculateBounds = true)
    {
        QRect bounds = fitContentParameters.bounds;

        if (recalculateBounds)
        {
            if (geometryByItem.isEmpty())
            {
                bounds = QRect(0, 0, 0, 0);
            }
            else
            {
                bounds = geometryByItem.begin().value();

                for (const auto& geometry: geometryByItem)
                    bounds = bounds.united(geometry);
            }
        }

        const QSizeF viewportSize = q->size();

        const QSizeF contentSize(bounds.width() * cellAspectRatio, bounds.height());
        const auto scale = Geometry::scaleFactor(contentSize, viewportSize);

        const GridViewportParameters params(bounds, scale);
        if (params != fitContentParameters)
        {
            const bool boundsChanged = bounds != fitContentParameters.bounds;

            fitContentParameters = params;
            emit q->fitContentsParametersChanged();
            if (!zoomedItem)
                emit q->defaultParametersChanged();

            if (boundsChanged)
                emit q->gridBoundsChanged();
        }

        const qreal cellScale = Geometry::scaleFactor(QSizeF(cellAspectRatio, 1), viewportSize);
        if (!qFuzzyCompare(oneCellScale, cellScale))
        {
            oneCellScale = cellScale;
            emit q->oneCellScaleChanged();
        }
    }

    void updateZoomedItemParameters()
    {
        const QRect& geometry = geometryByItem.value(zoomedItem);
        const QSizeF itemSize(geometry.width() * cellAspectRatio, geometry.height());
        const qreal scale = geometry.isNull() ? 1 : Geometry::scaleFactor(itemSize, q->size());

        const GridViewportParameters params(geometry, scale);
        if (fitZommedItemParameters != params)
        {
            fitZommedItemParameters = params;
            emit q->fitZoomedItemParameters();
            emit q->defaultParametersChanged();
        }
    }
};

GridViewport::GridViewport(QQuickItem* parent):
    QQuickItem(parent),
    d(new Private(this))
{
}

GridViewport::~GridViewport()
{
}

qreal GridViewport::cellAspectRatio() const
{
    return d->cellAspectRatio;
}

void GridViewport::setCellAspectRatio(qreal ratio)
{
    if (qFuzzyCompare(d->cellAspectRatio, ratio))
        return;

    d->cellAspectRatio = ratio;
    emit cellAspectRatioChanged();
}

QPointF GridViewport::viewCenter() const
{
    return d->center;
}

void GridViewport::setViewCenter(const QPointF& center)
{
    if (d->center == center)
        return;

    d->center = center;
    emit viewCenterChanged();

    d->positionItems();
}

qreal GridViewport::viewScale() const
{
    return d->scale;
}

void GridViewport::setViewScale(qreal scale)
{
    if (qFuzzyCompare(d->scale, scale))
        return;

    d->scale = scale;
    emit viewScaleChanged();

    d->positionItems();
}

GridViewportParameters GridViewport::fitContentsParameters() const
{
    return d->fitContentParameters;
}

GridViewportParameters GridViewport::fitZoomedItemParameters() const
{
    return d->fitZommedItemParameters;
}

GridViewportParameters GridViewport::defaultParameters() const
{
    return d->zoomedItem ? d->fitZommedItemParameters : d->fitContentParameters;
}

qreal GridViewport::oneCellScale() const
{
    return d->oneCellScale;
}

QRect GridViewport::gridBounds() const
{
    return d->fitContentParameters.bounds;
}

QQuickItem* GridViewport::zoomedItem() const
{
    return d->zoomedItem;
}

void GridViewport::setZoomedItem(QQuickItem* item)
{
    if (d->zoomedItem == item)
        return;

    d->zoomedItem = item;
    emit zoomedItemChanged();

    d->updateZoomedItemParameters();
}

QVector2D GridViewport::mapToViewport(const QVector2D& vector) const
{
    return QVector2D(
        (float) (vector.x() * d->cellAspectRatio * d->scale),
        (float) (vector.y() * d->scale));
}

QSizeF GridViewport::mapToViewport(const QSizeF& size) const
{
    return {size.width() * d->cellAspectRatio * d->scale, size.height() * d->scale};
}

QPointF GridViewport::mapToViewport(const QPointF& point) const
{
    const QPointF visualCenter(width() / 2, height() / 2);

    return Geometry::cwiseMul(
        point - d->center,
        QSizeF(d->cellAspectRatio * d->scale, d->scale)) + visualCenter;
}

QRectF GridViewport::mapToViewport(const QRectF& rect) const
{
    const QPointF visualCenter(width() / 2, height() / 2);

    return Geometry::cwiseMul(
        QRectF(rect).translated(-d->center),
                QSizeF(d->cellAspectRatio * d->scale, d->scale)).translated(visualCenter);
}

QVector2D GridViewport::mapFromViewport(const QVector2D& vector) const
{
    return QVector2D(
        (float) (vector.x() / (d->cellAspectRatio * d->scale)),
        (float) (vector.y() / d->scale));
}

QSizeF GridViewport::mapFromViewport(const QSizeF& size) const
{
    return {size.width() / (d->cellAspectRatio * d->scale), size.height() / d->scale};
}

QPointF GridViewport::mapFromViewport(const QPointF& point) const
{
    const QPointF visualCenter(width() / 2, height() / 2);

    return Geometry::cwiseDiv(
        point - visualCenter,
        QSizeF(d->cellAspectRatio * d->scale, d->scale)) + d->center;
}

QRectF GridViewport::mapFromViewport(const QRectF& rect) const
{
    const QPointF visualCenter(width() / 2, height() / 2);

    return Geometry::cwiseDiv(
        QRectF(rect).translated(-visualCenter),
        QSizeF(d->cellAspectRatio * d->scale, d->scale)).translated(-d->center);
}

GridViewportAttached* GridViewport::qmlAttachedProperties(QObject* object)
{
    return new GridViewportAttached(object);
}

void GridViewport::registerQmlType()
{
    qmlRegisterType<GridViewport>("nx.vms.client.core", 1, 0, "GridViewport");
    qRegisterMetaType<GridViewportParameters>();
}

void GridViewport::itemChange(ItemChange change, const ItemChangeData& data)
{
    switch (change)
    {
        case ItemChildAddedChange:
        {
            const auto attached = qobject_cast<GridViewportAttached*>(
                qmlAttachedPropertiesObject<GridViewport>(data.item));

            connect(attached, &GridViewportAttached::geometryChanged, this,
                [this, attached, item = data.item]
                {
                    d->updateItemGridGeometry(item, attached->geometry());
                });

            d->updateItemGridGeometry(data.item, attached->geometry());

            break;
        }

        case ItemChildRemovedChange:
            if (d->geometryByItem.remove(data.item))
            {
                d->updateFitContentsParameters();

                if (d->zoomedItem == data.item)
                    setZoomedItem(nullptr);
            }

            break;

        default:
            break;
    }

    QQuickItem::itemChange(change, data);
}

void GridViewport::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    d->updateFitContentsParameters(false);
    d->positionItems();
    if (d->zoomedItem)
        d->updateZoomedItemParameters();
}

} // namespace nx::vms::client::core
