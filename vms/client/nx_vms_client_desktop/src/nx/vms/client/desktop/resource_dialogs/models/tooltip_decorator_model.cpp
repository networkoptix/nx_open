// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tooltip_decorator_model.h"

namespace nx::vms::client::desktop {

TooltipDecoratorModel::TooltipDecoratorModel(QObject* parent):
    base_type(parent)
{
}

QVariant TooltipDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (!m_tooltipProvider || role != Qt::ToolTipRole || !index.isValid())
        return base_type::data(index, role);

    return m_tooltipProvider(index);
}

void TooltipDecoratorModel::setTooltipProvider(const TooltipProvider& tooltipProvider)
{
    m_tooltipProvider = tooltipProvider;
}

} // namespace nx::vms::client::desktop
