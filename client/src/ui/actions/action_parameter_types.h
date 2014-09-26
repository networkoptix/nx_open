#ifndef QN_ACTION_TARGET_TYPES_H
#define QN_ACTION_TARGET_TYPES_H

#include "action_fwd.h"

#include <utils/common/uuid.h>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>

#include "actions.h"

class QVariant;
class QGraphicsItem;

/**
 * Helper class that implements <tt>QVariant</tt>-based overloading for
 * the types supported by default action parameter.
 */
class QnActionParameterTypes {
public:
    static int size(const QVariant &items);

    static Qn::ActionParameterType type(const QVariant &items);

    static QnResourceList resources(const QVariant &items);

    static QnLayoutItemIndexList layoutItems(const QVariant &items);

    static QnVideoWallItemIndexList videoWallItems(const QVariant &items);

    static QnVideoWallMatrixIndexList videoWallMatrices(const QVariant &items);

    static QnWorkbenchLayoutList layouts(const QVariant &items);

    static QnResourceWidgetList widgets(const QVariant &items);



    static QnResourcePtr resource(QnResourceWidget *widget);

    static QnResourcePtr resource(QnWorkbenchLayout *layout);

    static QnResourceList resources(QnResourceWidget *widget);

    static QnResourceList resources(const QnResourceWidgetList &widgets);

    static QnResourceList resources(const QnLayoutItemIndexList &layoutItems);

    static QnResourceList resources(QnWorkbenchLayout *layout);

    static QnResourceList resources(const QnWorkbenchLayoutList &layouts);

    static QnLayoutItemIndex layoutItem(QnResourceWidget *widget);

    static QnLayoutItemIndexList layoutItems(const QnResourceWidgetList &widgets);

    static QnLayoutItemIndexList layoutItems(QnResourceWidget *widget);

    static QnResourceWidgetList widgets(const QList<QGraphicsItem *> items);
};

#endif // QN_ACTION_TARGET_TYPES_H
