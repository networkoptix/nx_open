#ifndef QN_LAYOUT_TAB_BAR_H
#define QN_LAYOUT_TAB_BAR_H

#include <QTabBar>

class QnWorkbenchLayout;

class QnLayoutTabBar: public QTabBar {
    Q_OBJECT;
public:
    QnLayoutTabBar(QWidget *parent = NULL);

    virtual ~QnLayoutTabBar();

    QnWorkbenchLayout *layout(int index) const;
    int indexOf(QnWorkbenchLayout *layout) const;

    void setCurrentLayout(QnWorkbenchLayout *layout);

    void addLayout(QnWorkbenchLayout *layout);
    void insertLayout(int index, QnWorkbenchLayout *layout);
    void removeLayout(QnWorkbenchLayout *layout);

signals:
    void layoutInserted(QnWorkbenchLayout *layout);
    void currentChanged(QnWorkbenchLayout *layout);
    void layoutRemoved(QnWorkbenchLayout *layout);

protected:
    virtual void tabInserted(int index) override;
    virtual void tabRemoved(int index) override;

    void updateTabsClosable();
    void updateCurrentLayout();

private slots:
    void at_tabCloseRequested(int index);
    void at_currentChanged(int index);
    void at_tabMoved(int from, int to);
    void at_layout_aboutToBeDestroyed();

private:
    void checkInvariants() const;

private:
    QList<QnWorkbenchLayout *> m_layouts;
    QnWorkbenchLayout *m_currentLayout;
};



#endif // QN_LAYOUT_TAB_BAR_H
