// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/client/desktop/resource_properties/schedule/schedule_cell_painter.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class BackupBandwidthScheduleCellPainter: public ScheduleCellPainter
{
    using base_type = ScheduleCellPainter;

public:
    BackupBandwidthScheduleCellPainter();
    virtual ~BackupBandwidthScheduleCellPainter() override;

    virtual void paintCellBackground(
        QPainter* painter,
        const QRectF& rect,
        bool hovered,
        const QVariant& cellData) const override;

    virtual void paintCellText(
        QPainter* painter,
        const QRectF& rect,
        const QVariant& cellData) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
