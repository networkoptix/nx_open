// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {
namespace ui {

namespace action { class Manager; }

namespace workbench {

class ResourceGroupingActionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ResourceGroupingActionHandler(QObject* parent = nullptr);

private:
    void createNewCustomResourceTreeGroup() const;
    void assignCustomResourceTreeGroupId() const;
    void renameCustomResourceTreeGroup() const;
    void moveToCustomResourceTreeGroup() const;
    void removeCustomResourceTreeGroup() const;

    void showMaximumDepthWarning() const;

private:
    action::Manager* menu() const;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
