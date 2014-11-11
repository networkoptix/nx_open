#ifndef WORKBENCH_LAYOUTS_HANDLER_H
#define WORKBENCH_LAYOUTS_HANDLER_H

#include <QtCore/QObject>

#include <QtWidgets/QMessageBox>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchStateDelegate;

class QnWorkbenchLayoutsHandler : public QObject, public QnWorkbenchContextAware
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

    void at_layouts_saved(int status, const QnResourceList &resources, int handle);

private:
    void saveLayout(const QnLayoutResourcePtr &layout);
    void saveLayoutAs(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user);

    /**
     * @brief alreadyExistingLayouts    Check if layouts with same name already exist.
     * @param name                      Suggested new name.
     * @param user                      User that will own the layout.
     * @param layout                    Layout that we want to rename (if any).
     * @return                          List of existing layouts with same name.
     */
    QnLayoutResourceList alreadyExistingLayouts(const QString &name, const QnUserResourcePtr &user, const QnLayoutResourcePtr &layout = QnLayoutResourcePtr());

    /**
     * @brief askOverrideLayout     Show messagebox asking user if he really wants to override existsing layout.
     * @param buttons               Messagebox buttons.
     * @param defaultButton         Default button.
     * @return                      Selected button.
     */
    QMessageBox::StandardButton askOverrideLayout(QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton);

    bool canRemoveLayouts(const QnLayoutResourceList &layouts);

    void removeLayouts(const QnLayoutResourceList &layouts);

    void closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources, QObject *target, const char *slot);
    bool closeLayouts(const QnLayoutResourceList &resources, bool waitForReply = false, bool force = false);
    bool closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply = false, bool force = false);

private:
    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
};

#endif // WORKBENCH_LAYOUTS_HANDLER_H
