#include "geometry.h"

#include <cmath>

#include <iterator>
#include <nx/utils/log/assert.h>

#include <boost/geometry/geometry.hpp>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace bg = boost::geometry;

namespace {

constexpr auto kBufferHoleThickness = 0.02f;
constexpr auto kBufferStripeThickness = 0.005f;
constexpr auto kBufferCirclePoints = 4;

using BoostPoint = bg::model::d2::point_xy<float>;
using BoostPolygon = bg::model::polygon<BoostPoint>;
using BoostMultiPolygon = bg::model::multi_polygon<BoostPolygon>;
using BoostLineString = bg::model::linestring<BoostPoint>;
using BoostMultiLineString = bg::model::multi_linestring<BoostLineString>;

BoostPoint toBoost(const Point& nxPoint)
{
    return {nxPoint.x, nxPoint.y};
}

BoostMultiPolygon toBoost(const Polygon& nxPolygon)
{
    BoostPolygon polygon;

    for (const auto& nxPoint: nxPolygon)
        polygon.outer().push_back(toBoost(nxPoint));

    bg::correct(polygon);

    return {polygon};
}

BoostMultiLineString toBoost(const Line& nxLine)
{
    BoostLineString line;

    for (const auto& nxPoint: nxLine)
        line.push_back(toBoost(nxPoint));

    return {line};
}

Point fromBoost(const BoostPoint& point)
{
    return {point.x(), point.y()};
}

Polygon fromBoost(const BoostPolygon& polygon)
{
    Polygon nxPolygon;

    for (const auto& point: polygon.outer())
        nxPolygon.push_back(fromBoost(point));

    return nxPolygon;
}

Line fromBoost(const BoostLineString& line)
{
    Line nxLine;

    for (const auto& point: line)
        nxLine.push_back(fromBoost(point));

    return nxLine;
}

template <typename Input, typename Output>
void simpleBuffer(const Input& input, Output& output, float amount)
{
    bg::buffer(input, output,
        bg::strategy::buffer::distance_symmetric(amount),
        bg::strategy::buffer::side_straight(),
        bg::strategy::buffer::join_round(kBufferCirclePoints),
        bg::strategy::buffer::end_round(kBufferCirclePoints),
        bg::strategy::buffer::point_circle(kBufferCirclePoints));
}

float computeDistance(const BoostPoint& a, const BoostPoint& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

float computeDistance(const Point& a, const Point& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

std::optional<BoostLineString> tryMerge(
    const BoostLineString& a, const BoostLineString& b, float maxDistance)
{
    if (!NX_ASSERT(!a.empty() && !b.empty()))
        return std::nullopt;

    if (computeDistance(a.back(), b.front()) <= maxDistance)
    {
        BoostLineString result;
        result.reserve(a.size() + b.size());
        result.insert(result.end(), a.begin(), a.end());
        result.insert(result.end(), b.begin(), b.end());
        return result;
    }

    if (computeDistance(a.front(), b.back()) <= maxDistance)
    {
        BoostLineString result;
        result.reserve(b.size() + a.size());
        result.insert(result.end(), b.begin(), b.end());
        result.insert(result.end(), a.begin(), a.end());
        return result;
    }

    if (computeDistance(a.back(), b.back()) <= maxDistance)
    {
        BoostLineString result;
        result.reserve(a.size() + b.size());
        result.insert(result.end(), a.begin(), a.end());
        result.insert(result.end(), 
            std::make_reverse_iterator(b.end()),
            std::make_reverse_iterator(b.begin()));
        return result;
    }

    if (computeDistance(a.front(), b.front()) <= maxDistance)
    {
        BoostLineString result;
        result.reserve(a.size() + b.size());
        result.insert(result.end(), 
            std::make_reverse_iterator(b.end()),
            std::make_reverse_iterator(b.begin()));
        result.insert(result.end(), a.begin(), a.end());
        return result;
    }

    return std::nullopt;
}

BoostMultiLineString merge(BoostMultiLineString lines, float maxDistance)
{
start:
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        for (std::size_t j = i + 1; j < lines.size(); ++j)
        {
            if (auto mergedLine = tryMerge(lines[i], lines[j], maxDistance))
            {
                lines[i] = std::move(*mergedLine);
                lines[j] = std::move(lines.back());
                lines.pop_back();

                goto start;
            }
        }
    }
    return lines;
}

} // namespace

Polygon Polygon::clipped(const Polygon& nxClipper) const
{
    auto polygon = toBoost(*this);
    const auto clipper = toBoost(nxClipper);

    BoostMultiPolygon union_;
    bg::union_(polygon, clipper, union_);

    BoostMultiPolygon unionHoles;
    for (auto& subPolygon: union_)
    {
        for (auto& hole: subPolygon.inners())
        {
            bg::reverse(hole);
            unionHoles.push_back({hole});
        }
    }

    BoostMultiPolygon paddedUnionHoles;
    simpleBuffer(unionHoles, paddedUnionHoles, kBufferHoleThickness);

    BoostMultiPolygon augmentedPolygon; 
    bg::union_(polygon, paddedUnionHoles, augmentedPolygon);

    BoostMultiPolygon result;
    bg::intersection(augmentedPolygon, clipper, result);

    if (result.empty())
        return {};

    return fromBoost(result[0]);
}

Polygon Polygon::simplified(std::size_t desiredSize) const
{
    // - Try to replace each pair of consecutive vertices with their midpoint.
    // - Actually replace the pair that changes the total area of the polygon the
    // least.
    // - Repeat until targget vertex count is reached.

    if (desiredSize == 0)
        return {};

    auto ring = toBoost(*this)[0].outer();

    decltype(ring) testRing;
    testRing.reserve(ring.size());

    while (ring.size() > desiredSize)
    {
        const auto area = bg::area(ring);

        std::size_t index = -1;
        float minAreaDifference = INFINITY;

        testRing.clear();
        testRing.insert(testRing.end(), ring.begin() + 1, ring.end());
        for (std::size_t i = 0; i < ring.size(); ++i)
        {
            const auto& point = ring[i];
            auto& testPoint = testRing[i % testRing.size()];

            bg::add_point(testPoint, point);
            bg::divide_value(testPoint, 2);

            const auto areaDifference = std::abs(area - bg::area(testRing));
            if (areaDifference < minAreaDifference)
            {
                index = i;
                minAreaDifference = areaDifference;
            }

            testPoint = point;
        }

        if (NX_ASSERT(index != (std::size_t) -1))
        {
            auto& point = ring[(index + 1) % ring.size()];

            bg::add_point(point, ring[index]);
            bg::divide_value(point, 2);

            ring.erase(ring.begin() + index);
        }
    }

    // At this point self-insersections are possible, but we do nothing to resolve
    // them, since camera can return self intersecting polygons regardless.

    return fromBoost(BoostPolygon{ring});
}

Line Line::clipped(const Polygon& nxClipper) const
{
    auto line = toBoost(*this);
    const auto clipper = toBoost(nxClipper);

    BoostMultiPolygon stripe;
    simpleBuffer(line, stripe, kBufferStripeThickness);

    BoostMultiPolygon union_;
    bg::union_(clipper, stripe, union_);

    BoostMultiPolygon unionHoles;
    for (auto& subPolygon: union_)
    {
        for (auto& hole: subPolygon.inners())
        {
            bg::reverse(hole);
            unionHoles.push_back({hole});
        }
    }

    BoostMultiPolygon paddedUnionHoles;
    simpleBuffer(unionHoles, paddedUnionHoles, kBufferStripeThickness);

    BoostMultiLineString clipperBoundaries;
    for (auto& polygon: clipper)
    {
        const auto& ring = polygon.outer();

        BoostLineString boundary(ring.begin(), ring.end());
        if (!ring.empty())
        {
            const auto& first = ring.front();
            const auto& last = ring.back();
            if (first.x() != last.x() || first.y() != last.y())
                boundary.push_back(first);
        }

        clipperBoundaries.push_back(std::move(boundary));
    }

    BoostMultiLineString linePatches;
    bg::intersection(clipperBoundaries, paddedUnionHoles, linePatches);

    BoostMultiLineString clippedLine;
    bg::intersection(clipper, line, clippedLine);

    for (auto& patch: linePatches)
        clippedLine.push_back(std::move(patch));

    line = merge(clippedLine, kBufferStripeThickness);

    if (line.empty())
        return {};

    // At this point self-insersections are possible, but we do nothing to resolve
    // them, since camera can return self intersecting lines regardless.

    return fromBoost(line[0]);
}

Line Line::simplified(std::size_t desiredSize) const
{
    // Essentially https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm
    // but reparametrized to terminate when desired vertex count is reached instead of maximum
    // allowed deviation distance.

    if (desiredSize == 0)
        return {};

    Line line = *this;
    while (line.size() > desiredSize)
    {
        if (line.size() == 2)
        {
            line[0].x += line[1].x;
            line[0].y += line[1].y;

            line[0].x /= 2;
            line[0].y /= 2;

            line.pop_back();

            break;
        }

        std::size_t nearestIndex = -1;
        float minDistance = INFINITY;

        const auto findLeastImportantPoint =
            [&](auto& findLeastImportantPoint, std::size_t firstIndex, std::size_t lastIndex)
            {
                if (lastIndex - firstIndex == 1)
                    return;

                const Point& first = line[firstIndex];
                const Point& last = line[lastIndex];

                const float chordC = first.y * last.x - first.x * last.y;
                const float chordA = (last.y - first.y) / chordC;
                const float chordB = (first.x - last.x) / chordC;

                const bool chordIsValid = std::isfinite(chordA) && std::isfinite(chordB);

                const auto computeDistance =
                    [&](const Point& point)
                    {
                        if (chordIsValid)
                            return std::abs(point.x * chordA + point.y * chordB + 1);
                        else
                            return vivotek::computeDistance(point, first);
                    };

                std::size_t furthestIndex = -1;
                float maxDistance = -INFINITY;
                for (std::size_t i = firstIndex + 1; i < lastIndex; ++i)
                {
                    const float distance = computeDistance(line[i]);
                    if (distance > maxDistance)
                    {
                        furthestIndex = i;
                        maxDistance = distance;
                    }
                }

                if (!NX_ASSERT(furthestIndex != (std::size_t) -1))
                    return;

                if (maxDistance < minDistance)
                {
                    nearestIndex = furthestIndex;
                    minDistance = maxDistance;
                }

                findLeastImportantPoint(findLeastImportantPoint, firstIndex, furthestIndex);
                findLeastImportantPoint(findLeastImportantPoint, furthestIndex, lastIndex);
            };

        findLeastImportantPoint(findLeastImportantPoint, 0, line.size() - 1);

        if (!NX_ASSERT(nearestIndex != (std::size_t) -1))
            break;

        line.erase(line.begin() + nearestIndex);
    }

    return line;
}

Line Line::subdivided(std::size_t desiredSize) const
{
    if (!NX_ASSERT(!empty()))
        return *this;

    Line line = *this;
    line.reserve(desiredSize);
    while (line.size() < desiredSize)
    {
        if (line.size() == 1)
        {
            line.push_back(line[0]);
            continue;
        }

        std::size_t longestIndex = -1;
        float maxDistance = -INFINITY;
        for (std::size_t i = 1; i < line.size(); ++i)
        {
            const Point& a = line[i - 1];
            const Point& b = line[i];

            const float distance = computeDistance(a, b);
            if (distance > maxDistance)
            {
                maxDistance = distance;
                longestIndex = i;
            }
        }

        if (!NX_ASSERT(longestIndex != (std::size_t) -1))
            break;

        const Point& a = line[longestIndex - 1];
        const Point& b = line[longestIndex];
        line.emplace(line.begin() + longestIndex, (a.x + b.x) / 2, (a.y + b.y) / 2);
    }

    return line;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
