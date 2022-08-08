// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "media_resource_widget.h"

class QnCrossSystemCameraWidget: public QnMediaResourceWidget
{
    Q_OBJECT

public:
    QnCrossSystemCameraWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);
    ~QnCrossSystemCameraWidget() override;

protected:
    int calculateButtonsVisibility() const override;
    Qn::ResourceStatusOverlay calculateStatusOverlay() const override;
    Qn::ResourceOverlayButton calculateOverlayButton(
        Qn::ResourceStatusOverlay statusOverlay) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
