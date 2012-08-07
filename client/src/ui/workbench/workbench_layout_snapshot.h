#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_H

#include <QtCore/QSizeF>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>

class QnWorkbenchLayoutSnapshot {
public:
    QnWorkbenchLayoutSnapshot(): cellAspectRatio(-1.0), cellSpacing(-1.0, -1.0) {}

    QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource);

    QnLayoutItemDataMap items;
    QString name;
    qreal cellAspectRatio;
    QSizeF cellSpacing;

    friend bool operator==(const QnWorkbenchLayoutSnapshot &l, const QnWorkbenchLayoutSnapshot &r);
};

#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_H
