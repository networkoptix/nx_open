
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;

class QnWorkbenchItemsWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchItemsWatcher(QObject *parent = nullptr);

    virtual ~QnWorkbenchItemsWatcher();

signals:
    void layoutChanged();
    void layoutAboutToBeChanged();

    void itemAdded(QnWorkbenchItem *item);
    void itemRemoved(QnWorkbenchItem *item);

    void itemShown(QnWorkbenchItem *item);
    void itemHidden(QnWorkbenchItem *item);
};