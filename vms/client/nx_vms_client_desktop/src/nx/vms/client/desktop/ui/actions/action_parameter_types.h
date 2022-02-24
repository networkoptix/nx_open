// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QList>

class QVariant;
class QGraphicsItem;

#include <nx/vms/client/desktop/ui/actions/action_types.h>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

class QnResourceWidget;
class QnMediaResourceWidget;
class QnServerResourceWidget;
class QnWorkbenchLayout;

typedef QList<QnResourceWidget*> QnResourceWidgetList;
typedef QList<QnWorkbenchLayout*> QnWorkbenchLayoutList;

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

/**
 * Helper class that implements <tt>QVariant</tt>-based overloading for
 * the types supported by default action parameter.
 */
class ParameterTypes
{
public:
    static int size(const QVariant& items);

    static nx::vms::client::desktop::ui::action::ActionParameterType type(const QVariant& items);
    static QnResourceList resources(const QVariant& items);
    static QnLayoutItemIndexList layoutItems(const QVariant& items);
    static QnVideoWallItemIndexList videoWallItems(const QVariant& items);
    static QnVideoWallMatrixIndexList videoWallMatrices(const QVariant& items);
    static QnWorkbenchLayoutList layouts(const QVariant& items);
    static QnResourceWidgetList widgets(const QVariant& items);
    static QnResourcePtr resource(QnResourceWidget* widget);
    static QnResourcePtr resource(QnWorkbenchLayout* layout);
    static QnResourceList resources(QnResourceWidget* widget);
    static QnResourceList resources(const QnResourceWidgetList& widgets);
    static QnResourceList resources(const QnLayoutItemIndexList& layoutItems);
    static QnResourceList resources(QnWorkbenchLayout* layout);
    static QnResourceList resources(const QnWorkbenchLayoutList& layouts);
    static QnLayoutItemIndex layoutItem(QnResourceWidget* widget);
    static QnLayoutItemIndexList layoutItems(const QnResourceWidgetList& widgets);
    static QnLayoutItemIndexList layoutItems(QnResourceWidget* widget);
    static QnResourceWidgetList widgets(const QList<QGraphicsItem*>& items);

    /** Debug string representation of the action parameter items. */
    static QString toString(const QVariant& items);
};

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
