#ifndef QN_LAYOUT_TAB_BAR_H
#define QN_LAYOUT_TAB_BAR_H

#include <QTabBar>

class QnWorkbench;
class QnWorkbenchLayout;

class QnLayoutTabBar: public QTabBar {
    Q_OBJECT;
public:
    QnLayoutTabBar(QWidget *parent = NULL);

    virtual ~QnLayoutTabBar();

    void setWorkbench(QnWorkbench *workbench);
    QnWorkbench *workbench() const;

signals:
    void closeRequested(QnWorkbenchLayout *layout);

protected:
    virtual void tabInserted(int index) override;
    virtual void tabRemoved(int index) override;

    void updateTabsClosable();
    void updateCurrentLayout();

private slots:
    void at_tabCloseRequested(int index);
    void at_currentChanged(int index);
    void at_tabMoved(int from, int to);
    void at_layout_nameChanged();
    void at_workbench_aboutToBeDestroyed();
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

    /** Workbench associated with this tab bar. */
    QnWorkbench *m_workbench;
};



#endif // QN_LAYOUT_TAB_BAR_H
