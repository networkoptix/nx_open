#pragma once

#include <ui/graphics/items/standard/graphics_qml_view.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class PtzPromoOverlay: public GraphicsQmlView
{
    Q_OBJECT

public:
    PtzPromoOverlay(QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    virtual ~PtzPromoOverlay() override;

signals:
    void closeRequested();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
