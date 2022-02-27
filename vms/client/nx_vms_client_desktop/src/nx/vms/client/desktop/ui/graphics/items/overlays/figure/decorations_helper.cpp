// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "decorations_helper.h"

#include <cmath>
#include <algorithm>

#include <QtGui/QPolygonF>
#include <QtGui/QTransform>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/geometry.h>

#include "polygon.h"
#include "polyline.h"

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop::figure {

namespace {

struct Zone
{
    qreal start = 0;
    qreal end = 0;

    qreal length() const
    {
        if (end > start)
            return end - start;

        NX_ASSERT(false, "Unexpected zone length.");
        return 0;
    }
};

struct IntersectionInfo
{
    QPointF point;
    QLineF::IntersectionType type;
};

struct IntersectionWithLine
{
    QPointF point;
    QLineF line;
};

const QPolygonF kZeroRect({{0, 0}, {0, 0}, {0, 0}, {0, 0}});

// For values ordering only. Excludes root calculation.
qreal distanceSquared(const QPointF& p1, const QPointF& p2)
{
    return pow(p2.x() - p1.x(), 2) + pow(p2.y() - p1.y(), 2);
}

bool isSamePoint(const QPointF& p1, const QPointF& p2)
{
    return abs(p1.x() - p2.x()) < 1.0 && abs(p1.y() - p2.y()) < 1.0;
}

QVector<IntersectionWithLine> getSegmentsIntersections(
    const QLineF& edge,
    const Lines& segmentsToIntersect)
{
    QVector<IntersectionWithLine> intersections;
    for (const QLineF& segment: segmentsToIntersect)
    {
        QPointF intersectionPoint;
        if (segment.intersect(edge, &intersectionPoint) == QLineF::BoundedIntersection)
        {
            const bool pointAlreadyFound =
                std::any_of(intersections.begin(), intersections.end(),
                [intersectionPoint](const IntersectionWithLine& intersection)
                {
                    return isSamePoint(intersectionPoint, intersection.point);
                });

            if (!pointAlreadyFound)
                intersections.append({intersectionPoint, segment});
        }
    }
    return intersections;
}

// Returns list of intersected rectangular zone projections on horizontal layout axe.
QVector<Zone> getForbiddenZoned(const QPolygonF& layoutRect, const Lines& edges)
{
    const QLineF top(layoutRect[0], layoutRect[1]);
    const QLineF right(layoutRect[1], layoutRect[2]);
    const QLineF bottom(layoutRect[3], layoutRect[2]);
    const QLineF left(layoutRect[3], layoutRect[0]);

    const qreal layoutWidth = top.length();

    QVector<Zone> forbiddenZones;

    const QVector<QLineF> segmentsToIntersect{top, right, bottom, left};

    for (const QLineF& edge: edges)
    {
        QVector<IntersectionWithLine> intersections =
            getSegmentsIntersections(edge, segmentsToIntersect);

        if (!NX_ASSERT(intersections.count() < 3, "Unexpected intersection number"))
            intersections = {intersections[0], intersections[1]};

        if (intersections.count() == 0)
            continue;

        // Fixate intersection order to simplify zone calculation.
        std::sort(intersections.begin(), intersections.end(),
            [&top, &right, &bottom, &left](
                const IntersectionWithLine& lhs,
                const IntersectionWithLine& rhs)
            {
                auto getOrder = [&top, &right, &bottom, &left](const QLineF& line)
                    {
                        if (line == left)
                            return 0;
                        if (line == right)
                            return 1;
                        if (line == top)
                            return 2;

                        NX_ASSERT(line == bottom, "Unexpected line");
                        return 3;
                    };

                return getOrder(lhs.line) < getOrder(rhs.line);
            });

        Zone newZone;
        if (intersections[0].line == left)
        {
            if (intersections.count() == 1)
            {
                if (layoutRect.contains(edge.p1()))
                    newZone = {0, Geometry::distanceToEdge(edge.p1(), left)};
                else if (layoutRect.contains(edge.p2()))
                    newZone = {0, Geometry::distanceToEdge(edge.p2(), left)};
                else
                    continue;
            }
            else
            {
                if (intersections[1].line == left)
                    continue;

                if (intersections[1].line == right)
                    newZone = {0, layoutWidth};
                else if (intersections[1].line == top)
                    newZone = {0, QLineF(layoutRect[0], intersections[1].point).length()};
                else // if (intersections[1].line == bottom)
                    newZone = {0, QLineF(layoutRect[3], intersections[1].point).length()};
            }
        }
        else if (intersections[0].line == right)
        {
            if (intersections.count() == 1)
            {
                if (layoutRect.contains(edge.p1()))
                    newZone = {Geometry::distanceToEdge(edge.p1(), left), layoutWidth};
                else if (layoutRect.contains(edge.p2()))
                    newZone = {Geometry::distanceToEdge(edge.p2(), left), layoutWidth};
                else
                    continue;
            }
            else
            {
                if (intersections[1].line == right)
                    continue;

                // Handled above:
                // if (intersections[1].line == left)

                if (intersections[1].line == top)
                    newZone = {QLineF(layoutRect[0], intersections[1].point).length(), layoutWidth};
                else // if (intersections[1].line == bottom)
                    newZone = {QLineF(layoutRect[3], intersections[1].point).length(), layoutWidth};
            }
        }
        else if (intersections[0].line == top)
        {
            qreal distance1 = 0;
            qreal distance2 = 0;

            if (intersections.count() == 1)
            {
                if (layoutRect.contains(edge.p1()))
                {
                    distance1 = QLineF(layoutRect[0], intersections[0].point).length();
                    distance2 = Geometry::distanceToEdge(edge.p1(), left);
                }
                else if (layoutRect.contains(edge.p2()))
                {
                    distance1 = QLineF(layoutRect[0], intersections[0].point).length();
                    distance2 = Geometry::distanceToEdge(edge.p2(), left);
                }
                else
                    continue;
            }
            else
            {
                // Handled above:
                // if (intersections[1].line == right)
                // if (intersections[1].line == left)

                if (intersections[1].line == top)
                {
                    distance1 = QLineF(layoutRect[0], intersections[0].point).length();
                    distance2 = QLineF(layoutRect[0], intersections[1].point).length();
                }
                else // if (intersections[1].line == bottom)
                {
                    distance1 = QLineF(layoutRect[0], intersections[0].point).length();
                    distance2 = QLineF(layoutRect[3], intersections[1].point).length();
                }
            }

            newZone = distance1 < distance2
                ? Zone{distance1, distance2}
                : Zone{distance2, distance1};
        }
        else if (intersections[0].line == bottom)
        {
            qreal distance1 = 0;
            qreal distance2 = 0;

            if (intersections.count() == 1)
            {
                if (layoutRect.contains(edge.p1()))
                {
                    distance1 = QLineF(layoutRect[3], intersections[0].point).length();
                    distance2 = Geometry::distanceToEdge(edge.p1(), left);
                }
                else if (layoutRect.contains(edge.p2()))
                {
                    distance1 = QLineF(layoutRect[3], intersections[0].point).length();
                    distance2 = Geometry::distanceToEdge(edge.p2(), left);
                }
                else
                    continue;
            }
            else
            {
                // Handled above:
                // if (intersections[1].line == right)
                // if (intersections[1].line == left)
                // if (intersections[1].line == top)

                //if (intersections[1].line == bottom)
                distance1 = QLineF(layoutRect[3], intersections[0].point).length();
                distance2 = QLineF(layoutRect[3], intersections[1].point).length();
            }

            newZone = distance1 < distance2
                ? Zone{distance1, distance2}
                : Zone{distance2, distance1};
        }

        forbiddenZones.append(newZone);
    }

    return forbiddenZones;
}

// Returns 0-4 intersection point.
QVector<IntersectionInfo> findSceneRectIntersectionPoints(
    const QLineF& segment,
    const QRectF& sceneRect)
{
    const QLineF leftBorder{{0, 0}, {0, sceneRect.height()}};
    const QLineF rightBorder{{sceneRect.width(), 0}, {sceneRect.width(), sceneRect.height()}};
    const QLineF topBorder{{0, 0}, {sceneRect.width(), 0}};
    const QLineF bottomBorder{{0, sceneRect.height()}, {sceneRect.width(), sceneRect.height()}};

    QVector<IntersectionInfo> result;

    IntersectionInfo info;

    info.type = segment.intersect(leftBorder, &info.point);
    if (info.type != QLineF::NoIntersection)
    {
        info.point.setX(0);
        if (sceneRect.contains(info.point))
            result.append(info);
    }

    info.type = segment.intersect(rightBorder, &info.point);
    if (info.type != QLineF::NoIntersection)
    {
        info.point.setX(sceneRect.width());
        if (sceneRect.contains(info.point))
            result.append(info);
    }

    info.type = segment.intersect(topBorder, &info.point);
    if (info.type != QLineF::NoIntersection)
    {
        info.point.setY(0);
        if (sceneRect.contains(info.point))
            result.append(info);
    }

    info.type = segment.intersect(bottomBorder, &info.point);
    if (info.type != QLineF::NoIntersection)
    {
        info.point.setY(sceneRect.height());
        if (sceneRect.contains(info.point))
            result.append(info);
    }

    return result;
}

std::optional<QLineF> calculateVisibleSegment(const QLineF& segment, const QRectF& sceneRect)
{
    const QVector<IntersectionInfo> sceneIntersections =
        findSceneRectIntersectionPoints(segment, sceneRect);

    Points visiblePoints;
    for (const auto& intersectionInfo: sceneIntersections)
    {
        if (intersectionInfo.type == QLineF::BoundedIntersection)
            visiblePoints.append(intersectionInfo.point);
    }

    if (sceneRect.contains(segment.p1()))
        visiblePoints.append(segment.p1());
    if (sceneRect.contains(segment.p2()))
        visiblePoints.append(segment.p2());

    std::sort(visiblePoints.begin(), visiblePoints.end(),
        [segment](const QPointF& p1, const QPointF& p2)
        {
            return distanceSquared(segment.p1(), p1) < distanceSquared(segment.p1(), p2);
        });

    // Remove excessive points, that may exist because of rounding.
    if (visiblePoints.count() > 1)
        return QLineF(visiblePoints.first(), visiblePoints.last());

    // Line intersects rect in a vertex.
    return {};
}

QPolygonF fitRectToScene(const QPolygonF& rect, const QRectF& sceneRect)
{
    const QLineF top(rect[0], rect[1]);
    const QLineF bottom(rect[3], rect[2]);

    const qreal rectWidth = top.length();
    if (rectWidth == 0)
        return kZeroRect;

    std::optional<QLineF> fittedTop = calculateVisibleSegment(top, sceneRect);
    std::optional<QLineF> fittedBottom = calculateVisibleSegment(bottom, sceneRect);
    if (!(fittedTop.has_value() && fittedBottom.has_value()))
        return kZeroRect;

    qreal leftOffset = std::max(
        QLineF(rect[0], fittedTop.value().p1()).length(),
        QLineF(rect[3], fittedBottom.value().p1()).length());

    qreal rightOffset = std::max(
        QLineF(rect[1], fittedTop.value().p2()).length(),
        QLineF(rect[2], fittedBottom.value().p2()).length());

    const qreal leftPos = leftOffset / rectWidth;
    const qreal rightPos = (rectWidth - rightOffset) / rectWidth;

    return {{
        top.pointAt(leftPos),
        top.pointAt(rightPos),
        bottom.pointAt(rightPos),
        bottom.pointAt(leftPos)
    }};
}

Lines cullInvisibleEdgeParts(const Lines& originalEdges, const QRectF& sceneRect)
{
    Lines resultEdges;

    for (const QLineF& edge: originalEdges)
    {
        const auto  visibleSegment = calculateVisibleSegment(edge, sceneRect);
        if (visibleSegment.has_value())
            resultEdges.append(visibleSegment.value());
    }

    return resultEdges;
}

qreal calculateLabelAngle(const QLineF& segment)
{
    const qreal segmentAngle = segment.angle();
    return -1 * // QTransform works with reflected Y-axis.
        ((0 < segmentAngle && segmentAngle <= 90) ? segmentAngle :
        (90 < segmentAngle && segmentAngle <= 180) ? segmentAngle - 180:
        (180 < segmentAngle && segmentAngle <= 270) ? segmentAngle - 180:
        segmentAngle);
}

bool checkLayoutSizeIsEnough(const QPolygonF& layoutRect, qreal labelWidth)
{
    return QLineF(layoutRect[0], layoutRect[1]).length() >= labelWidth;
}

/**
 * Creates rectangle, postioned to posTransform position and angle.
 *
 * @param reflected If the value is False,
 *     top left angle (point #0) is positioned to posTransform,
 *     otherwise, bottom left angle (point #3) is positioned to posTransform.
 */
QPolygonF createRect(const QSizeF& rectSize, const QTransform& posTransform, bool reflected = false)
{
    QPolygonF rect(QRectF{
        QPointF{0, 0},
        QPointF{rectSize.width(), rectSize.height() * (reflected ? -1 : 1)}});

    if (reflected)
        rect = QPolygonF({rect[3], rect[2], rect[1], rect[0]});

    return posTransform.map(rect);
}

/**
 * Fit layoutRect to the scene, also cuts any layout zones, intersected with edges.
 * Returns the biggest valid layout subrectangle.
 */
QPolygonF getAllowedRect(const QPolygonF& layoutRect, const Lines& edges, const QRectF& sceneRect)
{
    const QPolygonF& fittedLayoutRect = fitRectToScene(layoutRect, sceneRect);

    const QLineF top(fittedLayoutRect[0], fittedLayoutRect[1]);
    const QLineF bottom(fittedLayoutRect[3], fittedLayoutRect[2]);

    const qreal layoutRectWidth = top.length();
    if (layoutRectWidth == 0)
        return fittedLayoutRect;

    QVector<Zone> forbiddenZones = getForbiddenZoned(fittedLayoutRect, edges);

    std::sort(forbiddenZones.begin(), forbiddenZones.end(),
        [](const Zone& lhs, const Zone& rhs)
        {
            return lhs.start < rhs.start;
        });

    QVector<Zone> allowedZones;

    qreal allowedZoneStart = 0;
    for (const Zone& zone: forbiddenZones)
    {
        if (zone.start > allowedZoneStart)
            allowedZones.append({allowedZoneStart, zone.start});
        allowedZoneStart = zone.end;
    }

    if (allowedZoneStart < layoutRectWidth)
        allowedZones.append({allowedZoneStart, layoutRectWidth});

    if (allowedZones.isEmpty())
        return kZeroRect;

    int biggestAllowedZone = 0;
    for (int i = 1; i < allowedZones.size(); ++i)
    {
        if (allowedZones[biggestAllowedZone].length() < allowedZones[i].length())
            biggestAllowedZone = i;
    }

    qreal startPos = allowedZones[biggestAllowedZone].start / layoutRectWidth;
    qreal endPos = allowedZones[biggestAllowedZone].end / layoutRectWidth;

    return {{top.pointAt(startPos),
        top.pointAt(endPos),
        bottom.pointAt(endPos),
        bottom.pointAt(startPos)}};
}

/**
 * Return top left label rect point.
 */
QPointF calculateLabelPosition(
    const QPolygonF& layoutRect,
    qreal labelWidth,
    qreal topMargin)
{
    const QLineF top(layoutRect[0], layoutRect[1]);
    const QLineF right(layoutRect[1], layoutRect[2]);
    const QLineF left(layoutRect[0], layoutRect[3]);

    const qreal layoutWidth = top.length();
    const qreal layoutHeight = left.length();

    QLineF baseline;
    if (layoutHeight != 0)
    {
        baseline = QLineF(
            left.pointAt(topMargin / layoutHeight),
            right.pointAt(topMargin / layoutHeight));
    }
    else
    {
        baseline = top;
    }

    if (layoutWidth != 0)
        return baseline.pointAt(((layoutWidth - labelWidth) / 2) / layoutWidth);

    return top.p1();
}

Lines formPolylineEdges(const Points& originalPoints, int cuttingSegmentStartIndex, const QLineF& segmentToCut)
{
    Lines result;
    for (int i = 0; i < originalPoints.count() - 1; ++i)
    {
        const QPointF& p1 = originalPoints[i];
        const QPointF& p2 = originalPoints[i + 1];
        if (i == cuttingSegmentStartIndex)
        {
            result.append({p1, segmentToCut.p1()});
            result.append({segmentToCut.p2(), p2});
        }
        else
        {
            result.append({p1, p2});
        }
    }
    return result;
}

QLineF getDirectionMarksEdge(const QLineF& edge)
{
    if (edge.length() > 0)
    {
        const qreal factor1 = ((edge.length() - Polyline::kDirectionMarkWidth) / 2) / edge.length();
        const qreal factor2 = ((edge.length() + Polyline::kDirectionMarkWidth) / 2) / edge.length();
        return {edge.pointAt(factor1), edge.pointAt(factor2)};
    }
    return edge;
}

Lines formPolygonEdges(const Points& points)
{
    Lines result;
    for (int i = 0; i < points.count() - 1; ++i)
    {
        const QPointF& p1 = points[i];
        const QPointF& p2 = points[i + 1];
        result.append({p1, p2});
    }

    const QPointF& lastPoint = points[points.count() - 1];
    const QPointF& firstPoint = points[0];
    result.append({lastPoint, firstPoint});

    return result;
}

QPointF getAnchorPoint(const QLineF& segment)
{
    const qreal segmentAngle = segment.angle();
    return (90 < segmentAngle && segmentAngle < 270) ? segment.p2() : segment.p1();
}

bool checkPositionOutsideFigure(
    qreal testOffset,
    const QPolygonF& figurePolygon,
    const QLineF& figureEdge,
    const QTransform& posTransform)
{
    QPolygonF testFrame(QRectF(QPointF{0, 0}, QPointF{figureEdge.length() / 2, testOffset}));
    testFrame = posTransform.map(testFrame);
    return !figurePolygon.containsPoint(testFrame[2], Qt::OddEvenFill);
}

/**
 * Checks, if the figure inner part is "above" the edge.
 */
bool isDirectPositionOutsideFigure(
    const QPolygonF& figurePolygon,
    const QLineF& figureEdge,
    const QTransform& posTransform)
{
    static constexpr qreal kTestInsideRectOffset = 1;

    return checkPositionOutsideFigure(
        kTestInsideRectOffset,
        figurePolygon,
        figureEdge,
        posTransform);
}
/**
 * Checks, if the figure inner part is "below" the edge.
 */
bool isReflectedPositionOutsideFigure(
    const QPolygonF& figurePolygon,
    const QLineF& figureEdge,
    const QTransform& posTransform)
{
    static constexpr qreal kReflectedTestInsideRectOffset = -1;

    return checkPositionOutsideFigure(
        kReflectedTestInsideRectOffset,
        figurePolygon,
        figureEdge,
        posTransform);
}

Lines getEdges(const FigurePtr& figure, const Points& originalPoints, const QSizeF& scale)
{
    // Divide edge with direction marks on two parts.
    if (const Polyline* polyline = dynamic_cast<Polyline*>(figure.get()))
    {
        const QPointF directionsEdgeStartPoint =
            originalPoints[directionMarksEdgeStartPointIndex(polyline)];
        const QPointF directionsEdgeEndPoint =
            originalPoints[directionMarksEdgeStartPointIndex(polyline) + 1];
        const QLineF segmentToCut = getDirectionMarksEdge(
            {directionsEdgeStartPoint, directionsEdgeEndPoint});

        return formPolylineEdges(
            originalPoints,
            directionMarksEdgeStartPointIndex(polyline),
            segmentToCut);
    }

    if (dynamic_cast<Polygon*>(figure.get()))
        return formPolygonEdges(originalPoints);

    return {};
}

} // namespace

std::optional<LabelPosition> getLabelPosition(
    const QSizeF& scale,
    FigurePtr figure,
    const QSizeF& labelSize,
    qreal topMargin,
    qreal bottomMargin)
{
    auto figurePolygon = QPolygonF(figure->points(scale));
    figurePolygon.translate(figure->pos(scale));

    const Lines edges = getEdges(figure, figurePolygon, scale);

    if (!NX_ASSERT(!edges.empty(), "Invalid edges number"))
        return {};

    const auto sceneRect = QRectF{{0, 0}, scale};

    const bool isClosedShape =
        dynamic_cast<nx::vms::client::desktop::figure::ClosedShapeFigure*>(figure.get());

    Lines visibleEdges = cullInvisibleEdgeParts(edges, sceneRect);
    std::sort(visibleEdges.begin(), visibleEdges.end(),
        [](const QLineF& lhs, const QLineF& rhs)
        {
            return lhs.length() > rhs.length(); // Descending order.
        });

    // Try to place label along the longest edges.
    for (const auto& edge: visibleEdges)
    {
        if (edge.length() < labelSize.width())
            break;

        Lines otherEdges(visibleEdges);
        otherEdges.removeAll(edge);

        const qreal labelAngle = calculateLabelAngle(edge);
        const QPointF anchorPoint = getAnchorPoint(edge);
        const QTransform posTransform =
            QTransform().translate(anchorPoint.x(), anchorPoint.y()).rotate(labelAngle);

        // Try to position label below edge.
        if (!isClosedShape || isDirectPositionOutsideFigure(figurePolygon, edge, posTransform))
        {
            const QSizeF labelLayoutSize(edge.length(), labelSize.height() + topMargin);
            const QPolygonF layoutBelowEdge = createRect(labelLayoutSize, posTransform);
            const QPolygonF allowedLayoutBelowEdge =
                getAllowedRect(layoutBelowEdge, otherEdges, sceneRect);
            if (checkLayoutSizeIsEnough(allowedLayoutBelowEdge, labelSize.width()))
            {
                const QPointF labelPos = calculateLabelPosition(
                    allowedLayoutBelowEdge,
                    labelSize.width(),
                    topMargin);
                return LabelPosition{labelPos, labelAngle};
            }
        }

        // Try to position label above edge.
        if (!isClosedShape || isReflectedPositionOutsideFigure(figurePolygon, edge, posTransform))
        {
            const QSizeF labelLayoutSize(edge.length(), labelSize.height() + bottomMargin);
            const QPolygonF layoutAboveEdge = createRect(labelLayoutSize, posTransform, true);
            const QPolygonF allowedLayoutAboveEdge =
                getAllowedRect(layoutAboveEdge, otherEdges, sceneRect);
            if (checkLayoutSizeIsEnough(allowedLayoutAboveEdge, labelSize.width()))
            {
                const QPointF labelPos = calculateLabelPosition(
                    allowedLayoutAboveEdge,
                    labelSize.width(),
                    0);
                return LabelPosition{labelPos, labelAngle};
            }
        }
    }

    // Not enough place for label.
    return {};
}

int directionMarksEdgeStartPointIndex(const Polyline* figure)
{
    return figure->pointsCount() / 2 - 1;
}

} // namespace nx::vms::client::desktop::figure
