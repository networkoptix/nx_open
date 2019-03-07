#pragma once

#include "../interfaces.h"

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop::integrations::internal {

class RoiVisualizationIntegration: public Integration, public IOverlayPainter
{
    Q_OBJECT
    using base_type = Integration;

public:
    explicit RoiVisualizationIntegration(QObject* parent = nullptr);
    virtual void registerWidget(QnMediaResourceWidget* widget) override;
    virtual void unregisterWidget(QnMediaResourceWidget* widget) override;
    virtual void paintVideoOverlay(QnMediaResourceWidget* widget, QPainter* painter) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::integrations::internal
