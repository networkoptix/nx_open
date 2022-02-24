// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

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

NX_REFLECTION_INSTRUMENT_ENUM(Direction, none, left, right, both)

struct NX_VMS_CLIENT_DESKTOP_API Item
{
    Points points;
    QColor color;

    bool operator==(const Item& other) const = default;

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

    bool operator==(const Line& other) const = default;
    virtual bool isValid() const override;
};
#define Line_Fields Item_Fields(direction)

using Lines = std::vector<Line>;

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::client::desktop::integrations::internal::roi::Box,
    (json),
    NX_VMS_CLIENT_DESKTOP_API)

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::client::desktop::integrations::internal::roi::Polygon,
    (json),
    NX_VMS_CLIENT_DESKTOP_API)

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::client::desktop::integrations::internal::roi::Line,
    (json),
    NX_VMS_CLIENT_DESKTOP_API)

} // namespace nx::vms::client::desktop::integrations::internal::roi
