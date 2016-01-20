
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;

class QnCurrentLayoutItemsWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnCurrentLayoutItemsWatcher(QObject *parent = nullptr);

    virtual ~QnCurrentLayoutItemsWatcher();

signals:
    void layoutChanged();
    void layoutAboutToBeChanged();
    void itemAdded(QnWorkbenchItem *item);
    void itemRemoved(QnWorkbenchItem *item);
};