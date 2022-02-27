// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_H

#include <QtCore/QSizeF>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>

// TODO: #sivanov Replace with nx::vms::api::LayoutData.
class QnWorkbenchLayoutSnapshot
{
public:
    QnWorkbenchLayoutSnapshot();

    QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource);

    QnLayoutItemDataMap items;
    QString name;
    qreal cellAspectRatio;
    qreal cellSpacing;

    QSize backgroundSize;
    QString backgroundImageFilename;
    qreal backgroundOpacity;
    QSize fixedSize;
    qint32 logicalId = 0;

    bool locked;

    friend bool operator==(const QnWorkbenchLayoutSnapshot &l, const QnWorkbenchLayoutSnapshot &r);
};

#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_H
