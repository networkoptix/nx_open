// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/vms/client/core/media/analytics_fwd.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/types.h>

class QnMediaResourceWidget;
namespace nx::analytics::db { struct Filter; }

namespace nx::vms::client::desktop {

class AnalyticsOverlayWidget;

class WidgetAnalyticsController: public QObject
{
    using base_type = QObject;
    using Filter = nx::analytics::db::Filter;

public:
    WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget);
    virtual ~WidgetAnalyticsController() override;

    void updateAreas(std::chrono::microseconds timestamp, int channel);
    void clearAreas();

    void setAnalyticsMetadataProvider(const core::AbstractAnalyticsMetadataProviderPtr& provider);
    void setAnalyticsOverlayWidget(AnalyticsOverlayWidget* widget);
    const Filter& filter() const;
    void setFilter(const Filter& value);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
