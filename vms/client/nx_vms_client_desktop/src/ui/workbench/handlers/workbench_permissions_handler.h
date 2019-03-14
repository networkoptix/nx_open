#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

#include <ui/workbench/workbench_state_manager.h>

class QnWorkbenchLayout;

using QnWorkbenchLayoutList = QList<QnWorkbenchLayout*>;

namespace nx::vms::client::desktop {

class PermissionsHandler: public QObject, public QnSessionAwareDelegate
{
    Q_OBJECT

public:
    explicit PermissionsHandler(QObject* parent = nullptr);
    virtual ~PermissionsHandler();

    // Overrides from from QnSessionAwareDelegate.
    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

private:
    void at_shareLayoutAction_triggered();
    void at_stopSharingLayoutAction_triggered();
    void at_shareCameraAction_triggered();

private:
    void shareLayoutWith(const QnLayoutResourcePtr& layout, const QnResourceAccessSubject& subject);

    void shareCameraWith(const QnVirtualCameraResourcePtr& camera, const QnResourceAccessSubject& subject);

    bool confirmStopSharingLayouts(const QnResourceAccessSubject& subject, const QnLayoutResourceList& layouts);
};

} // namespace nx::vms::client::desktop
