// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

class QPainter;
class QRectF;
class QVariant;

namespace nx::vms::client::desktop {

class ScheduleCellPainter
{
public:
    ScheduleCellPainter();
    virtual ~ScheduleCellPainter();

    virtual void paintCellBackground(
        QPainter* painter,
        const QRectF& rect,
        bool hovered,
        const QVariant& cellData) const;

    virtual void paintCellText(
        QPainter* painter,
        const QRectF& rect,
        const QVariant& cellData) const;

    virtual void paintSelectionFrame(QPainter* painter, const QRectF& rect) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
