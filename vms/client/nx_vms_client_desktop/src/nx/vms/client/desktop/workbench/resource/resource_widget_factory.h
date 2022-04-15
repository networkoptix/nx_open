// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QnResourceWidget;
class QnWorkbenchItem;

namespace nx::vms::client::desktop {

namespace ui {
namespace workbench {

class ResourceWidgetFactory
{
public:
    static QnResourceWidget* createWidget(QnWorkbenchItem* item);
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
