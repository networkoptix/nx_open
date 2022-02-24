// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QFlags>

#include <nx/vms/client/desktop/resource_properties/schedule/schedule_cell_painter.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class RecordScheduleCellPainter: public ScheduleCellPainter
{
    using base_type = ScheduleCellPainter;

public:
    enum DisplayOption
    {
        NoOptions = 0x0,
        ShowQuality = 0x1,
        ShowFps = 0x2,
    };
    Q_DECLARE_FLAGS(DisplayOptions, DisplayOption);

    RecordScheduleCellPainter(DisplayOptions = {});
    virtual ~RecordScheduleCellPainter() override;

    void setDisplayOptions(const DisplayOptions& options);

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

Q_DECLARE_OPERATORS_FOR_FLAGS(RecordScheduleCellPainter::DisplayOptions);

} // namespace nx::vms::client::desktop
