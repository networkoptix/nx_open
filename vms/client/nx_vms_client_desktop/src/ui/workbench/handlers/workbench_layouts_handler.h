// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

#include <ui/workbench/workbench_state_manager.h>
#include <nx/vms/event/event_fwd.h>

class QnWorkbenchLayout;
typedef QList<QnWorkbenchLayout *> QnWorkbenchLayoutList;

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class LayoutsHandler: public QObject, public QnSessionAwareDelegate
{
    Q_OBJECT
public:
    explicit LayoutsHandler(QObject *parent = 0);
    virtual ~LayoutsHandler();

    void renameLayout(const QnLayoutResourcePtr &layout, const QString &newName);
    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

private:
    void at_newUserLayoutAction_triggered();
    void at_saveLayoutAction_triggered();
    void at_saveCurrentLayoutAction_triggered();
    void at_saveLayoutAsCloudAction_triggered();
    void at_saveLayoutForCurrentUserAsAction_triggered();
    void at_saveCurrentLayoutAsAction_triggered();
    void at_saveCurrentLayoutAsCloudAction_triggered();
    void at_closeLayoutAction_triggered();
    void at_closeAllButThisLayoutAction_triggered();
    void at_removeFromServerAction_triggered();
    void at_openNewTabAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_removeLayoutItemFromSceneAction_triggered();
    void at_openLayoutAction_triggered(const vms::event::AbstractActionPtr& businessAction);
    void at_forgetLayoutPasswordAction_triggered();

private:
    bool closeAllLayouts(bool force = false);

    /**
     * Save target file, local or remote layout.
     * @param layout Layout to save.
     */
    void saveLayout(const QnLayoutResourcePtr& layout);

    /** Save a copy of remote layout for the current user. */
    void saveRemoteLayoutAs(const QnLayoutResourcePtr& layout);

    /** Save common remote layout as a cloud one. */
    void saveLayoutAsCloud(const QnLayoutResourcePtr& layout);

    /** Save existing cloud layout under another name. */
    void saveCloudLayoutAs(const QnLayoutResourcePtr& layout);

    void removeLayoutItems(const QnLayoutItemIndexList& items, bool autoSave);

    struct LayoutChange
    {
        QnLayoutResourcePtr layout;
        QnResourceList added;
        QnResourceList removed;
    };

    LayoutChange calculateLayoutChange(const QnLayoutResourcePtr& layout);

    /** Ask user if layout should be saved. Actual when admin modifies shared layout
     *  or layout belonging to user with custom access rights.
     */
    bool confirmLayoutChange(const LayoutChange& change, const QnResourcePtr& layoutOwner);

    bool confirmChangeSharedLayout(const LayoutChange& change);
    bool confirmDeleteSharedLayouts(const QnLayoutResourceList& layouts);
    bool confirmChangeLocalLayout(const QnUserResourcePtr& user, const LayoutChange& change);
    bool confirmDeleteLocalLayouts(const QnUserResourcePtr& user, const QnLayoutResourceList& layouts);
    bool confirmChangeVideoWallLayout(const LayoutChange& change);

    /** If user has custom access rights, he must be given direct access to cameras on changed local layout. */
    void grantMissingAccessRights(const QnUserResourcePtr& user, const LayoutChange& change);

    bool canRemoveLayouts(const QnLayoutResourceList &layouts);

    void removeLayouts(const QnLayoutResourceList &layouts);

    void closeLayoutsInternal(
        const QnLayoutResourceList& resources,
        const QnLayoutResourceList& rollbackResources);
    bool closeLayouts(const QnLayoutResourceList& resources, bool force = false);
    bool closeLayouts(const QnWorkbenchLayoutList& layouts, bool force = false);
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
