#ifndef QN_ACTION_TARGET_TYPES_H
#define QN_ACTION_TARGET_TYPES_H

#include "action_fwd.h"
#include <QUuid>
#include <QSet>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include "actions.h"

class QVariant;
class QGraphicsItem;

class QnActionTargetTypes {
public:
    static int resourceList();

    static int widgetList();

    static int layoutItemList();

    static void initialize();

    static int size(const QVariant &items);

    static Qn::ActionTarget target(const QVariant &items);

    static QnResourcePtr resource(QnResourceWidget *widget);

    static QnResourceList resources(const QnResourceWidgetList &widgets);

    static QnResourceList resources(const QnLayoutItemIndexList &layoutItems);

    static QnResourceList resources(const QVariant &items);

    static QnLayoutItemIndexList layoutItems(const QnResourceWidgetList &widgets);

    static QnLayoutItemIndexList layoutItems(const QVariant &items);

    static QnResourceWidgetList widgets(const QVariant &items);

    static QnResourceWidgetList widgets(const QList<QGraphicsItem *> items);

};

Q_DECLARE_METATYPE(QnResourceWidgetList);

#endif // QN_ACTION_TARGET_TYPES_H
