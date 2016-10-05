#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

#include <ui/workbench/workbench_context_aware.h>

class QnSessionAwareDelegate;

class QnWorkbenchLayoutsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchLayoutsHandler(QObject *parent = 0);
    virtual ~QnWorkbenchLayoutsHandler();

    void renameLayout(const QnLayoutResourcePtr &layout, const QString &newName);
    bool closeAllLayouts(bool waitForReply = false, bool force = false);
    bool tryClose(bool force);
    void forcedUpdate();

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
    void at_shareLayoutAction_triggered();
    void at_stopSharingLayoutAction_triggered();
    void at_openNewTabAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_removeLayoutItemFromSceneAction_triggered();

    void at_workbench_layoutsChanged();

private:

    /** Save target file, local or remote layout. */
    void saveLayout(const QnLayoutResourcePtr &layout);

    void saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user);

    void removeLayoutItems(const QnLayoutItemIndexList& items, bool autoSave);

    void shareLayoutWith(const QnLayoutResourcePtr &layout, const QnResourceAccessSubject &subject);

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
    bool confirmLayoutChange(const LayoutChange& change);

    bool confirmChangeSharedLayout(const LayoutChange& change);
    bool confirmDeleteSharedLayouts(const QnLayoutResourceList& layouts);
    bool confirmChangeLocalLayout(const QnUserResourcePtr& user, const LayoutChange& change);
    bool confirmDeleteLocalLayouts(const QnUserResourcePtr& user, const QnLayoutResourceList& layouts);
    bool confirmStopSharingLayouts(const QnResourceAccessSubject& subject, const QnLayoutResourceList& layouts);

    /** If user has custom access rights, he must be given direct access to cameras on changed local layout. */
    void grantMissingAccessRights(const QnUserResourcePtr& user, const LayoutChange& change);

    bool canRemoveLayouts(const QnLayoutResourceList &layouts);

    void removeLayouts(const QnLayoutResourceList &layouts);

    void closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources);
    bool closeLayouts(const QnLayoutResourceList &resources, bool waitForReply = false, bool force = false);
    bool closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply = false, bool force = false);

    void at_layout_saved(bool success, const QnLayoutResourcePtr &layout);
private:
    QScopedPointer<QnSessionAwareDelegate> m_workbenchStateDelegate;

    /** Flag that we are in layouts closing process. */
    bool m_closingLayouts;
};
