// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "camera_button.h"

namespace nx::vms::client::desktop {

class TwoWayAudioButton: public CameraButton
{
    using base_type = CameraButton;

public:
    TwoWayAudioButton(QGraphicsItem* parent = nullptr);
    virtual ~TwoWayAudioButton() override;

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
