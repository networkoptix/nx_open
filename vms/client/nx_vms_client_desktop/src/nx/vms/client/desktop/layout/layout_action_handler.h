// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/vms/event/event_fwd.h>

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

    void renameLayout(const LayoutResourcePtr& layout, const QString& newName);

private:
    void at_newUserLayoutAction_triggered();
    void at_closeLayoutAction_triggered();
    void at_closeAllButThisLayoutAction_triggered();
    void at_removeFromServerAction_triggered();
    void at_openNewTabAction_triggered();
    void at_openIntercomLayoutAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_removeLayoutItemFromSceneAction_triggered();
    void at_businessActionReceived(const vms::event::AbstractActionPtr& businessAction);
    void at_forgetLayoutPasswordAction_triggered();
    void at_openInNewTabAction_triggered();

    void onRenameResourceAction();

private:
    /**
     * Save target file, local or remote layout.
     * @param layout Layout to save.
     */
    void saveLayout(const LayoutResourcePtr& layout);

    /**
     * Save provided layout under another name. Remote layout will be copied for the current user.
     */
    void saveLayoutAs(const LayoutResourcePtr& layout);

    /** Save a copy of remote layout for the current user. */
    void saveRemoteLayoutAs(const LayoutResourcePtr& layout);

    /** Save common remote layout as a cloud one. */
    void saveLayoutAsCloud(const LayoutResourcePtr& layout);

    /** Save existing cloud layout under another name. */
    void saveCloudLayoutAs(const LayoutResourcePtr& layout);

    void removeLayoutItems(const LayoutItemIndexList& items, bool autoSave);

    struct LayoutChange
    {
        LayoutResourcePtr layout;
        QnResourceList added;
        QnResourceList removed;
    };

    LayoutChange calculateLayoutChange(const LayoutResourcePtr& layout);

    /** Ask user if layout should be saved. Actual when admin modifies shared layout
     *  or layout belonging to user with custom access rights.
     */
    bool confirmLayoutChange(const LayoutChange& change, const QnResourcePtr& layoutOwner);

    bool confirmChangeSharedLayout(const LayoutChange& change);
    bool confirmChangeVideoWallLayout(const LayoutChange& change);

    bool canRemoveLayouts(const LayoutResourceList& layouts);

    void removeLayouts(const LayoutResourceList& layouts);

    bool closeLayouts(const LayoutResourceList& resources);
    bool closeLayouts(const QnWorkbenchLayoutList& layouts);

    /**
     * Open provided layouts in new tabs.
     * @param state Playback synchronization state for the layout if it is not opened yet.
     */
    void openLayouts(
        const LayoutResourceList& layouts,
        const StreamSynchronizationState& playbackState);

    QString generateUniqueLayoutName(const QnUserResourcePtr& user) const;
};

} // namespace nx::vms::client::desktop
