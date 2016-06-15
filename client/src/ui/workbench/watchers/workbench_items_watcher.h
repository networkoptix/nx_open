
#pragma once

#include <QtCore/QHash>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/disconnect_helper.h>

class QnWorkbenchLayout;

class QnWorkbenchLayoutItemsWatcher : public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(int itemsCount READ itemsCount NOTIFY itemsCountChanged)

    typedef QnWorkbenchContextAware base_type;

public:
    QnWorkbenchLayoutItemsWatcher(QObject *parent);

    virtual ~QnWorkbenchLayoutItemsWatcher();

    int itemsCount() const;

signals:
    void itemsCountChanged();

private:
    void setItemsCount(int count);

    void incItemsCount(int diff = 1);
    
    void decItemsCount(int diff = 1);

    void onLayoutAdded(QnWorkbenchLayout *layout);

    void onLayoutRemoved(QnWorkbenchLayout *layout);

private:
    typedef QHash<QnWorkbenchLayout *, QnDisconnectHelperPtr> ConnectionsData;

    ConnectionsData m_connections;
    int m_itemsCount;
};