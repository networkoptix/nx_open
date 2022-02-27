// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsBlurEffect>

namespace nx::vms::client::desktop {

/**
 * The same as QGraphicsBlurEffect, but effect rect is bounded by source geometry,
 * i.e. picture outside source is not affected by the effect.
 * Due to QGraphicsBlurEffect implementation details, effective right and bottom edge coordinates
 * can be 1px greater than corresponding source edge.
 */
class BoundedBlurEffect: public QGraphicsBlurEffect
{
    Q_OBJECT

public:
    using QGraphicsBlurEffect::QGraphicsBlurEffect;

    virtual QRectF boundingRectFor(const QRectF& rect) const override;
};

} // nx::vms::client::desktop
