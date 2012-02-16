#ifndef QN_ACTION_META_TYPES_H
#define QN_ACTION_META_TYPES_H

#include "action_fwd.h"
#include <core/resource/resource_fwd.h>

class QVariant;
class QGraphicsItem;

class QnActionMetaTypes {
public:
    static int resourceList();

    static int widgetList();

    static void initialize();

    static int size(const QVariant &items);

    static QnResourcePtr resource(QnResourceWidget *widget);

    static QnResourceList resources(const QnResourceWidgetList &widgets);

    static QnResourceList resources(const QVariant &items);

    static QnResourceWidgetList widgets(const QVariant &items);

    static QnResourceWidgetList widgets(const QList<QGraphicsItem *> items);
};

Q_DECLARE_METATYPE(QnResourceWidgetList);

#endif // QN_ACTION_META_TYPES_H
