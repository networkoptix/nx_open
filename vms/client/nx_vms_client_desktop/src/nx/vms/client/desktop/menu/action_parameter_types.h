// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

#include "action_types.h"

class QVariant;
class QGraphicsItem;
class QnResourceWidget;
class QnMediaResourceWidget;
class QnServerResourceWidget;
class QnWorkbenchLayout;

typedef QList<QnResourceWidget*> QnResourceWidgetList;
typedef QList<QnWorkbenchLayout*> QnWorkbenchLayoutList;

namespace nx::vms::client::desktop {
namespace menu {

/**
 * Helper class that implements <tt>QVariant</tt>-based overloading for
 * the types supported by default action parameter.
 */
class ParameterTypes
{
public:
    static int size(const QVariant& items);

    static nx::vms::client::desktop::menu::ActionParameterType type(const QVariant& items);
    static QnResourceList resources(const QVariant& items);
    static LayoutItemIndexList layoutItems(const QVariant& items);
    static QnVideoWallItemIndexList videoWallItems(const QVariant& items);
    static QnVideoWallMatrixIndexList videoWallMatrices(const QVariant& items);
    static QnWorkbenchLayoutList layouts(const QVariant& items);
    static QnResourceWidgetList widgets(const QVariant& items);
    static QnResourcePtr resource(QnResourceWidget* widget);
    static QnResourcePtr resource(QnWorkbenchLayout* layout);
    static QnResourceList resources(QnResourceWidget* widget);
    static QnResourceList resources(const QnResourceWidgetList& widgets);
    static QnResourceList resources(const LayoutItemIndexList& layoutItems);
    static QnResourceList resources(QnWorkbenchLayout* layout);
    static QnResourceList resources(const QnWorkbenchLayoutList& layouts);
    static QnResourceList videowalls(const QVariant& items);
    static QnResourceList videowalls(const QnVideoWallItemIndexList& items);
    static QnResourceList videowalls(const QnVideoWallMatrixIndexList& matrices);
    static LayoutItemIndex layoutItem(QnResourceWidget* widget);
    static LayoutItemIndexList layoutItems(const QnResourceWidgetList& widgets);
    static LayoutItemIndexList layoutItems(QnResourceWidget* widget);
    static QnResourceWidgetList widgets(const QList<QGraphicsItem*>& items);

    /** Debug string representation of the action parameter items. */
    static QString toString(const QVariant& items);
};

} // namespace menu
} // namespace nx::vms::client::desktop
