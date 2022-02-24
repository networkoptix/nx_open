// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_filter_model.h"

#include <nx/vms/client/desktop/vms_rules/vms_rules_model_view/vms_rules_list_model.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

VmsRulesFilterModel::VmsRulesFilterModel(QObject* parent /*= nullptr*/):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterRole(VmsRulesModelRoles::searchStringsRole);
}

void VmsRulesFilterModel::setFilterString(const QString& string)
{
    if (string.trimmed() == m_filterString)
        return;

    m_filterString = string.trimmed();
    invalidateFilter();
}

bool VmsRulesFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_filterString.isEmpty())
        return true;

    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto searchStrings =
        sourceIndex.data(VmsRulesModelRoles::searchStringsRole).value<QStringList>();

    return std::any_of(std::cbegin(searchStrings), std::cend(searchStrings),
        [this] (const QString& string)
        {
            return string.contains(m_filterString, filterCaseSensitivity());
        });
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
