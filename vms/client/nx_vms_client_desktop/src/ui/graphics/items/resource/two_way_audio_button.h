// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "jump_to_live_button.h"

namespace nx::vms::client::desktop {

class TwoWayAudioButton: public JumpToLiveButton
{
    using base_type = JumpToLiveButton;
    Q_OBJECT

public:
    TwoWayAudioButton(QGraphicsItem* parent = nullptr);
    virtual ~TwoWayAudioButton() override;

    virtual void setToolTip(const QString& toolTip) override;

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
