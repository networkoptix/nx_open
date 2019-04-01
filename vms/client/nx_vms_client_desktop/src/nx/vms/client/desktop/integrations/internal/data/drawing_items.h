#pragma once

#include <QtGui/QColor>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::client::desktop::integrations::internal::roi {

using Point = std::vector<double>;
using Points = std::vector<Point>;

enum class Direction
{
    none = 0,
    left = 1 << 0,
    right = 1 << 1,
    both = left | right
};

struct Item
{
    Points points;
    QColor color;

    virtual ~Item() = default;
    virtual bool isValid() const;
};
#define Item_Fields (points)(color)

struct NX_VMS_CLIENT_DESKTOP_API Box: Item
{
    virtual bool isValid() const override;
};
#define Box_Fields Item_Fields

struct NX_VMS_CLIENT_DESKTOP_API Polygon: Item
{
    virtual bool isValid() const override;
};
#define Polygon_Fields Item_Fields

struct NX_VMS_CLIENT_DESKTOP_API Line: Item
{
    Direction direction = Direction::none;

    virtual bool isValid() const override;
};
#define Line_Fields Item_Fields(direction)

using Lines = std::vector<Line>;

QN_FUSION_DECLARE_FUNCTIONS(
	nx::vms::client::desktop::integrations::internal::roi::Direction,
    (lexical),
    NX_VMS_CLIENT_DESKTOP_API)

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::client::desktop::integrations::internal::roi::Box,
    (json)(eq),
    NX_VMS_CLIENT_DESKTOP_API)

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::client::desktop::integrations::internal::roi::Polygon,
    (json)(eq),
    NX_VMS_CLIENT_DESKTOP_API)

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::client::desktop::integrations::internal::roi::Line,
    (json)(eq),
    NX_VMS_CLIENT_DESKTOP_API)

} // namespace nx::vms::client::desktop::integrations::internal::roi
