#ifndef QN_LAYOUT_TAB_BAR_H
#define QN_LAYOUT_TAB_BAR_H

#include <QtWidgets/QTabBar>
#include <core/resource/resource_fwd.h>
#include <ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchContext;
class QnWorkbenchLayout;
class QnWorkbench;

class QnLayoutTabBar: public QTabBar, public QnWorkbenchContextAware, public QnActionTargetProvider {
    Q_OBJECT
    typedef QTabBar base_type;

public:
    QnLayoutTabBar(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);

    virtual ~QnLayoutTabBar();

    virtual Qn::ActionScope currentScope() const override;

signals:
    void closeRequested(QnWorkbenchLayout *layout);
    void tabTextChanged();

protected:
    virtual QSize minimumSizeHint() const override;

    virtual void contextMenuEvent(QContextMenuEvent *event) override;

    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    virtual void tabInserted(int index) override;
    virtual void tabRemoved(int index) override;

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    QString layoutText(QnWorkbenchLayout *layout) const;
    QIcon layoutIcon(QnWorkbenchLayout *layout) const;

    void updateCurrentLayout();
    void updateTabText(QnWorkbenchLayout *layout);
    void updateTabIcon(QnWorkbenchLayout *layout);

private slots:
    void at_tabCloseRequested(int index);
    void at_currentChanged(int index);
    void at_tabMoved(int from, int to);
    
    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource);
    
    void at_workbench_layoutsChanged();
    void at_workbench_currentLayoutChanged();
    
private:
    void checkInvariants() const;
    void submitCurrentLayout();

private:
    /** Whether changes to tab bar should be written back into the workbench. */
    bool m_submit;

    /** Whether changes to the workbench should be written into this tab bar. */
    bool m_update;

    /** Tab-to-layout mapping. */
    QList<QnWorkbenchLayout *> m_layouts;

    /** Context associated with this tab bar. */
    QnWorkbenchContext *m_context;

    /** Tab we are trying to close with middle mouse button */
    int m_midClickedTab;
};



#endif // QN_LAYOUT_TAB_BAR_H
