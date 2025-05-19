// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnWorkbenchLayout;
typedef QList<QnWorkbenchLayout *> QnWorkbenchLayoutList;

namespace nx::vms::client::desktop {

struct StreamSynchronizationState;

class LayoutActionHandler: public QObject, public WindowContextAware
{
    Q_OBJECT

public:
    explicit LayoutActionHandler(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~LayoutActionHandler();

    void renameLayout(const core::LayoutResourcePtr& layout, const QString& newName);

private:
    void at_newUserLayoutAction_triggered();
    void at_closeLayoutAction_triggered();
    void at_closeAllButThisLayoutAction_triggered();
    void at_removeFromServerAction_triggered();
    void at_openNewTabAction_triggered();
    void at_openIntercomLayoutAction_triggered();
    void at_openMissedCallIntercomLayoutAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_removeLayoutItemFromSceneAction_triggered();
    void at_forgetLayoutPasswordAction_triggered();
    void at_openInNewTabAction_triggered();

    void onRenameResourceAction();
    void onNewUserLayoutNameChoosen(const QString& name, const QnUserResourcePtr& user);

private:
    /**
     * Save target file, local or remote layout.
     * @param layout Layout to save.
     */
    void saveLayout(const core::LayoutResourcePtr& layout);

    /**
     * Save provided layout under another name. Remote layout will be copied for the current user.
     */
    void saveLayoutAs(const core::LayoutResourcePtr& layout);

    /** Save a copy of remote layout for the current user. */
    void saveRemoteLayoutAs(const core::LayoutResourcePtr& layout);

    /** Save common remote layout as a cloud one. */
    void saveLayoutAsCloud(const core::LayoutResourcePtr& layout);

    /** Save existing cloud layout under another name. */
    void saveCloudLayoutAs(const core::LayoutResourcePtr& layout);

    void convertLayoutToShared(const core::LayoutResourcePtr& layout);

    void removeLayoutItems(const LayoutItemIndexList& items, bool autoSave);

    struct LayoutChange
    {
        core::LayoutResourcePtr layout;
        QnResourceList added;
        QnResourceList removed;
    };

    LayoutChange calculateLayoutChange(const core::LayoutResourcePtr& layout);

    /** Ask user if layout should be saved. Actual when admin modifies shared layout
     *  or layout belonging to user with custom access rights.
     */
    bool confirmLayoutChange(const LayoutChange& change, const QnResourcePtr& layoutOwner);

    bool confirmChangeSharedLayout(const LayoutChange& change);
    bool confirmChangeVideoWallLayout(const LayoutChange& change);

    bool canRemoveLayouts(const core::LayoutResourceList& layouts);

    void removeLayouts(const core::LayoutResourceList& layouts);

    bool closeLayouts(const core::LayoutResourceList& resources);
    bool closeLayouts(const QnWorkbenchLayoutList& layouts);

    /**
     * Open provided layouts in new tabs.
     * @param playbackState Playback synchronization state for the layout if it is not opened yet.
     * @param forceStateUpdate If true, the state of the layout's items will be updated forcefully,
     *     ignoring their previous state.
     */
    void openLayouts(
        const core::LayoutResourceList& layouts,
        const StreamSynchronizationState& playbackState,
        bool forceStateUpdate = false);

    QString generateUniqueLayoutName(const QnUserResourcePtr& user) const;
};

} // namespace nx::vms::client::desktop
