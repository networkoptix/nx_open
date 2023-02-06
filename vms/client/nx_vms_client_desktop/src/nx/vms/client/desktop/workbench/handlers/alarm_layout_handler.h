// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/rules/action_executor.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchLayout;
class QnWorkbenchItem;

namespace nx::vms::client::desktop {

class AlarmLayoutHandler:
    public nx::vms::rules::ActionExecutor,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = nx::vms::rules::ActionExecutor;

public:
    AlarmLayoutHandler(QObject* parent = nullptr);
    virtual ~AlarmLayoutHandler() override;

private:
    void openCamerasInAlarmLayout(
        const QnVirtualCameraResourceList& cameras,
        bool switchToLayoutNeeded,
        qint64 positionUs);

    void disableSyncForLayout(QnWorkbenchLayout* layout);
    void adjustLayoutCellAspectRatio(QnWorkbenchLayout* layout);
    QnWorkbenchLayout* findOrCreateAlarmLayout();
    bool alarmLayoutExists() const;
    void setCameraItemPosition(
        QnWorkbenchLayout* layout,
        QnWorkbenchItem* item,
        qint64 positionUs);

    virtual void execute(const nx::vms::rules::ActionPtr& action) override;

    /**
     *  Check if current client instance is the main one.
     *  Main instance is the first that was started on the given PC.
     */
    bool currentInstanceIsMain() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
