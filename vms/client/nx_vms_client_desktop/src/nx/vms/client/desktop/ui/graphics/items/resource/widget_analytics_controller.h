#pragma once

#include <chrono>

#include <nx/client/core/media/analytics_fwd.h>

class QnMediaResourceWidget;
namespace nx::analytics::db { struct Filter; }

namespace nx::vms::client::desktop {

class AreaHighlightOverlayWidget;

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
    void setAreaHighlightOverlayWidget(AreaHighlightOverlayWidget* widget);

    const Filter& filter() const;
    void setFilter(const Filter& value);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
