// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/widget_with_hint.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

/** Base class for the pickers with label on the left side and content on the right. */
class PlainPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    PlainPickerWidget(SystemContext* context, CommonParamsWidget* parent);

    void setReadOnly(bool value) override;

protected:
    WidgetWithHint<QnElidedLabel>* m_label{nullptr};
    QWidget* m_contentWidget{nullptr};

    void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
