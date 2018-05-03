#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

#include <ui/workbench/workbench_state_manager.h>

class QnWorkbenchLayout;
typedef QList<QnWorkbenchLayout *> QnWorkbenchLayoutList;

namespace nx {
namespace client {
namespace desktop {
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
    void at_saveLayoutAsAction_triggered();
    void at_saveLayoutForCurrentUserAsAction_triggered();
    void at_saveCurrentLayoutAsAction_triggered();
    void at_closeLayoutAction_triggered();
    void at_closeAllButThisLayoutAction_triggered();
    void at_removeFromServerAction_triggered();
    void at_openNewTabAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_removeLayoutItemFromSceneAction_triggered();

private:
    bool closeAllLayouts(bool force = false);

    /** Save target file, local or remote layout. */
    void saveLayout(const QnLayoutResourcePtr &layout);

    void saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user);

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
} // namespace desktop
} // namespace client
} // namespace nx
