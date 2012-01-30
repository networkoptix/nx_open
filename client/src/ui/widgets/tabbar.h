#ifndef TABBAR_H
#define TABBAR_H

#include <QtGui/QTabBar>

class TabBar : public QTabBar
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit TabBar(QWidget *parent = 0);

Q_SIGNALS:
    void tabInserted(int index, int); // second param is just to avoid naming collisions
    void tabRemoved(int index, int); // second param is just to avoid naming collisions
    void countChanged(int count);

protected:
    void tabInserted(int index);
    void tabRemoved(int index);

private Q_SLOTS:
    void _q_removeTab(int index);

private:
    Q_DISABLE_COPY(TabBar)
};

#endif // TABBAR_H
