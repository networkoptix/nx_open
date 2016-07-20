#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchStateDelegate;

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

protected:
    ec2::AbstractECConnectionPtr connection2() const;

    private slots:
    void at_newUserLayoutAction_triggered();
    void at_saveLayoutAction_triggered();
    void at_saveCurrentLayoutAction_triggered();
    void at_saveLayoutAsAction_triggered();
    void at_saveLayoutForCurrentUserAsAction_triggered();
    void at_saveCurrentLayoutAsAction_triggered();
    void at_closeLayoutAction_triggered();
    void at_closeAllButThisLayoutAction_triggered();
    void at_shareLayoutAction_triggered();
    void at_stopSharingLayoutAction_triggered();
    void at_openNewTabAction_triggered();

    void at_workbench_layoutsChanged();

private:
    void saveLayout(const QnLayoutResourcePtr &layout);
    void saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user);

    /** Ask user if layout should be saved. Actual when admin modifies shared layout
     *  or layout belonging to user with custom access rights.
     */
    bool confirmLayoutChange(const QnLayoutResourcePtr &layout);

    /**
     * @brief askOverrideLayout     Show message box asking user if he really wants to override existing layout.
     * @param buttons               Message box buttons.
     * @param defaultButton         Default button.
     * @return                      Selected button.
     */
    QDialogButtonBox::StandardButton askOverrideLayout(QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton);

    bool canRemoveLayouts(const QnLayoutResourceList &layouts);

    void removeLayouts(const QnLayoutResourceList &layouts);

    void closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources);
    bool closeLayouts(const QnLayoutResourceList &resources, bool waitForReply = false, bool force = false);
    bool closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply = false, bool force = false);

    void at_layout_saved(bool success, const QnLayoutResourcePtr &layout);
private:
    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;

    /** Flag that we are in layouts closing process. */
    bool m_closingLayouts;
};
