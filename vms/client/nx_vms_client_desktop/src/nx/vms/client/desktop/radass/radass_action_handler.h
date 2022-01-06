// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/radass/radass_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;

namespace nx::vms::client::desktop {

class RadassActionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    RadassActionHandler(QObject* parent = nullptr);
    virtual ~RadassActionHandler() override;

private:
    void at_radassAction_triggered();
    void handleItemModeChanged(const LayoutItemIndex& item, RadassMode mode);
    void handleCurrentLayoutAboutToBeChanged();
    void handleCurrentLayoutChanged();
    void handleLocalSystemIdChanged();
    void notifyAboutPerformanceLoss();
    void handleItemAdded(QnWorkbenchItem* item);

private:
    struct Private;
    QScopedPointer<Private> d;

};

} // namespace nx::vms::client::desktop
