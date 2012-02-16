#ifndef QN_ACTION_META_TYPES_H
#define QN_ACTION_META_TYPES_H

#include "action_fwd.h"
#include <core/resource/resource_fwd.h>

class QVariant;

class QnActionMetaTypes {
public:
    static int resourceList();

    static int graphicsItemList();

    static void initialize();

    static int size(const QVariant &items);

    static QnResourcePtr resource(QGraphicsItem *item);

    static QnResourceList resources(const QGraphicsItemList &items);

    static QnResourceList resources(const QVariant &items);

    static QGraphicsItemList graphicsItems(const QVariant &items);
};

Q_DECLARE_METATYPE(QGraphicsItemList);

#endif // QN_ACTION_META_TYPES_H
