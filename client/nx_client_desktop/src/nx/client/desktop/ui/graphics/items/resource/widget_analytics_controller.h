#pragma once

#include <nx/client/core/media/analytics_fwd.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace nx {
namespace client {
namespace desktop {

class AreaHighlightOverlayWidget;

class WidgetAnalyticsController: public QObject, public QnConnectionContextAware
{
    using base_type = QObject;

public:
    WidgetAnalyticsController(QnMediaResourceWidget* mediaResourceWidget);
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
