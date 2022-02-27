// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "types.h"

#include <QtGui/QColor>
#include <QtCore/QString>
#include <QtCore/QRectF>

#include <nx/vms/client/core/utils/rotation.h>

namespace nx::vms::client::desktop::figure {

/**
 * Base class for all figures. Figures may be drawn by the outer drawer on some scene or
 * its part. Also figure supports rotation for the target scene.
 */
class Figure
{
public:
    virtual ~Figure();

    FigureType type() const;
    QColor color() const;

    /**
     * Checks if bounding rect of the figure intersects with the given one. Specified rectangle
     * is supposed to be in the relateive (from 0 to 1) coordinates.
     */
    bool intersects(const QRectF& rect) const;

    /**
     * Represents rectangle of the target scene in relative coordintaes.
     * Default scene rectangle is Rect((0,0), (1, 1)).
     * After custom scene rectangle is set all coordinates become updated.
     */
    void setSceneRect(const QRectF& value);
    QRectF sceneRect() const;

    /** Sets rotation of the scene around its center point. */
    void setSceneRotation(core::StandardRotation value);
    core::StandardRotation sceneRotation() const;

    /**
     * Returns position of the top left corner for the bounding rectangle
     * in the global coordinates.
     * @param scale Scale to be applied to the returning point.
     */
    QPointF pos(const QSizeF& scale) const;

    /**
     * Returns points with specified scale in the local coordinates.
     * @param scale Scale to be applied to the each point.
     */
    Points points(const QSizeF& scale) const;

    /** Returns count of the points for the figure. */
    int pointsCount() const;

    /**
      * Returns rectangle which bounds all the points of the figure in local coordinate system.
      * @param scale Scale to be applied to returned rectangle.
      */
    QRectF boundingRect(const QSizeF& scale) const;

    /**
     * Checks if the point lays inside the figure. Derived figures may override
     * this function to implement custom behaviour, for example for small objects or
     * non-closed shape figures.
     * @param point Point to be check if it lays inside the figure.
     * Should be in the local coordinate system
     * @param scale Scale to be applied to the points before check.
     */
    virtual bool containsPoint(
        const Points::value_type& point,
        const QSizeF& scale) const;

    /**
     * Returns figure-specific rectangle which represents visible bounding rectangle,
     * in local coordinate system. Derived figures may return any visual rectangle,
     * even unrelated to the actual points.
     * @param scale Scale to be appied to the rectangle.
     * @return Rectangle, which width and height != 0.
     */
    virtual QRectF visualRect(const QSizeF& scale) const;

    /** Returns if the figure is valid. Derived types may override this logic. */
    virtual bool isValid() const;

    /** Equality compare function. */
    virtual bool equalsTo(const Figure& right) const;

    /** Clones figure. */
    virtual FigurePtr clone() const = 0;

protected:
    /**
     * Constructor
     * @param type Type of the figure.
     * @param points Points for figure in the global coordinate system.
     * @param color Color of the figure.
     * @param targetRect Rectangle of the target scene in the global relative coordinate system.
     * @param sceneRotation Rotation of the target scene.
     */
    Figure(
        const FigureType type,
        const Points& points,
        const QColor& color,
        const QRectF& sceneRect,
        core::StandardRotation sceneRotation);

    /** Returns initial, untouched points. */
    const Points& sourcePoints() const;

private:
    void updatePointValues();

private:
    const FigureType m_type;
    const Points m_sourcePoints;
    const QColor m_color;

    core::StandardRotation m_sceneRotation = core::StandardRotation::rotate0;
    QRectF m_sceneRect;

    /** Top left corner of the bounding rectangle in relative local coordinates. */
    Points::value_type m_targetPos;

    /** Points which are modified according to the scene rectangle and rotation. */
    Points m_targetPoints;

    /** Bounding rectangle which is modified according to the scene rectangle and rotation. */
    QRectF m_targetBoundingRect;
};

bool operator==(const Figure& left, const Figure& right);
bool operator!=(const Figure& left, const Figure& right);

/** Creation helper function. */
template<typename Result, typename SourcePoints, typename ...Args>
FigurePtr make(
    const SourcePoints& points,
    const QColor& color,
    Args ...args)
{
    return FigurePtr(new Result(points, color, args...));
}

} // namespace nx::vms::client::desktop::figure
