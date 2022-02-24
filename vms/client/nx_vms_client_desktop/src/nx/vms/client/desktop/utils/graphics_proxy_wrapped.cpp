// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "graphics_proxy_wrapped.h"

namespace nx::vms::client::desktop {

void UnbodiedGraphicsProxyWidget::setTransparentForMouseEvents(bool transparent)
{
    m_transparent = transparent;
    setAcceptedMouseButtons(m_transparent ? Qt::NoButton : Qt::AllButtons);
}

QPainterPath UnbodiedGraphicsProxyWidget::shape() const
{
    if (m_transparent)
        return QPainterPath();

    return base_type::shape();
}

} // namespace nx::vms::client::desktop
