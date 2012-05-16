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

class QnActionParameterTypes {
public:
    static void initialize();

    static int size(const QVariant &items);

    static Qn::ActionParameterType type(const QVariant &items);

    static QnResourcePtr resource(QnResourceWidget *widget);

    static QnResourceList resources(QnResourceWidget *widget);

    static QnResourceList resources(const QnResourceWidgetList &widgets);

    static QnResourceList resources(const QnLayoutItemIndexList &layoutItems);

    static QnResourceList resources(const QnWorkbenchLayoutList &layouts);

    static QnResourceList resources(const QVariant &items);

    static QnLayoutItemIndex layoutItem(QnResourceWidget *widget);

    static QnLayoutItemIndexList layoutItems(const QnResourceWidgetList &widgets);

    static QnLayoutItemIndexList layoutItems(QnResourceWidget *widget);

    static QnLayoutItemIndexList layoutItems(const QVariant &items);

    static QnWorkbenchLayoutList layouts(const QVariant &items);

    static QnResourceWidgetList widgets(const QVariant &items);

    static QnResourceWidgetList widgets(const QList<QGraphicsItem *> items);

};

Q_DECLARE_METATYPE(QnResourceWidgetList);
Q_DECLARE_METATYPE(QnWorkbenchLayoutList);

#endif // QN_ACTION_TARGET_TYPES_H
