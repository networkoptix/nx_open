// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/graphics/items/standard/graphics_qml_view.h>

namespace nx::vms::client::desktop {

class PtzPromoOverlay: public GraphicsQmlView
{
    Q_OBJECT

public:
    PtzPromoOverlay(QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    virtual ~PtzPromoOverlay() override;

    void setPagesVisibility(bool showBasics, bool showTargetLock);

signals:
    void closeRequested();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
