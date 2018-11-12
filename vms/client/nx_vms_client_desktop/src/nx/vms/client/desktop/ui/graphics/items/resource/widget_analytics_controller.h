#pragma once

#include <chrono>

#include <nx/client/core/media/analytics_fwd.h>

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class AreaHighlightOverlayWidget;

class WidgetAnalyticsController: public QObject
{
    using base_type = QObject;

public:
    WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget);
    virtual ~WidgetAnalyticsController() override;

    void updateAreas(std::chrono::microseconds timestamp, int channel);
    void clearAreas();

    void updateAreaForZoomWindow();

    void setAnalyticsMetadataProvider(const core::AbstractAnalyticsMetadataProviderPtr& provider);
    void setAreaHighlightOverlayWidget(AreaHighlightOverlayWidget* widget);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
