#pragma once

#include <nx/client/core/media/analytics_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class AreaHighlightOverlayWidget;

class WidgetAnalyticsController
{
public:
    WidgetAnalyticsController();
    virtual ~WidgetAnalyticsController();

    void updateAreas(qint64 timestamp, int channel);
    void clearAreas();

    void setAnalyticsMetadataProvider(const core::AbstractAnalyticsMetadataProviderPtr& provider);
    void setAreaHighlightOverlayWidget(AreaHighlightOverlayWidget* widget);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
