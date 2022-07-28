// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/impl_ptr.h>

#include "time_marker.h"

class QnTimeSlider;

namespace nx::vms::client::desktop::workbench::timeline {

class ThumbnailLoadingManager;

class LivePreview: public TimeMarker
{
    Q_OBJECT
    using base_type = TimeMarker;

public:
    explicit LivePreview(QnTimeSlider* slider, QObject* parent = nullptr);
    virtual ~LivePreview() override;

    void showAt(
        const QPoint& globalCursorPos,
        const QRectF& globalTimeSliderRect,
        const TimeContent& timeContent);

    void setThumbnailLoadingManager(ThumbnailLoadingManager* value);

    /** Number of adjacent pixels corresponding to the same preview thumbnail when hovered. */
    static constexpr int kStepPixels = 4;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::workbench::timeline
