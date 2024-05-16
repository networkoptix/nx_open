// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "select_analytics_object_types_button.h"

#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

SelectAnalyticsObjectTypesButton::SelectAnalyticsObjectTypesButton(QWidget* parent):
    QPushButton(parent)
{
    setProperty(nx::style::Properties::kPushButtonMargin, nx::style::Metrics::kStandardPadding);
}

void SelectAnalyticsObjectTypesButton::setSelectedObjectTypes(
    SystemContext* context,
    const QStringList &ids)
{
    if (ids.empty())
    {
        setIcon({});
        setText(tr("No objects"));
    }
    else if (ids.size() == 1)
    {
        const nx::analytics::taxonomy::AbstractObjectType* objectType =
            context->analyticsTaxonomyState()->objectTypeById(ids[0]);

        const QString iconPath =
            core::analytics::IconManager::instance()->absoluteIconPath(objectType->icon());

        setIcon(QIcon(iconPath));
        setText(objectType->name());
    }
    else
    {
        setIcon({});
        setText(tr("%n objects selected", /*comment*/ "", ids.size()));
    }
}

void SelectAnalyticsObjectTypesButton::setSelectedAllObjectTypes()
{
    setIcon({});
    setText(tr("All objects"));
}

} // namespace nx::vms::client::desktop
