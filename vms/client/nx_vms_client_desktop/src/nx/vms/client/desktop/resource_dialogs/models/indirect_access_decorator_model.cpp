// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "indirect_access_decorator_model.h"

#include <core/resource/layout_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/html/html.h>

namespace {

static const int kMaxTooltipResourceLines = 10;

} // namespace

namespace nx::vms::client::desktop {

IndirectAccessDecoratorModel::IndirectAccessDecoratorModel(
    nx::core::access::ResourceAccessProvider* accessProvider,
    int indirectAccessIconColumn,
    QObject* parent)
    :
    base_type(parent),
    m_resourceAccessProvider(accessProvider),
    m_indirectAccessIconColumn(indirectAccessIconColumn)
{
    NX_ASSERT(m_resourceAccessProvider);
}

void IndirectAccessDecoratorModel::setSubject(const QnResourceAccessSubject& subject)
{
    if (m_subject == subject)
        return;

    m_subject = subject;

    emit dataChanged(
        index(0, m_indirectAccessIconColumn),
        index(rowCount() - 1, m_indirectAccessIconColumn));
}

QVariant IndirectAccessDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !m_resourceAccessProvider || !m_subject.isValid()
        || index.column() != m_indirectAccessIconColumn)
    {
        return base_type::data(index, role);
    }

    const auto sourceIndex = mapToSource(index);
    const auto sourceResourceIndex = sourceIndex.siblingAtColumn(
        accessible_media_selection_view::ResourceColumn);

    const auto resource = sourceResourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!resource)
        return base_type::data(index, role);

    if (role == Qt::DecorationRole)
    {
        const auto accessLevels = m_resourceAccessProvider->accessLevels(m_subject, resource);
        if (accessLevels.contains(nx::core::access::Source::layout))
            return qnResIconCache->icon(QnResourceIconCache::SharedLayout);

        if (accessLevels.contains(nx::core::access::Source::videowall))
            return qnResIconCache->icon(QnResourceIconCache::VideoWall);
    }

    if (role == Qt::ToolTipRole)
    {
        QnResourceList providers;
        m_resourceAccessProvider->accessibleVia(m_subject, resource, &providers);
        return getTooltipRichText(providers);
    }

    return base_type::data(index, role);
}

QString IndirectAccessDecoratorModel::getTooltipRichText(const QnResourceList& providers) const
{
    if (providers.empty())
        return {};

    QStringList providerNames;
    for (const auto& provider: providers)
    {
        if (auto layout = provider.dynamicCast<QnLayoutResource>())
        {
            if (layout->isServiceLayout())
                continue;
        }
        providerNames.push_back(provider->getName());
    }
    providerNames.removeDuplicates();

    QStringList tooltipLines;
    tooltipLines.push_back(tr("Access granted by:"));

    for (int i = 0; i < std::min(kMaxTooltipResourceLines, providerNames.size()); ++i)
        tooltipLines.push_back(nx::vms::common::html::bold(providerNames.at(i)));

    if (providerNames.size() > kMaxTooltipResourceLines)
    {
        tooltipLines.push_back(
            tr("...and %n more", "", providerNames.size() - kMaxTooltipResourceLines));
    }

    return tooltipLines.join("<br>");
}

} // namespace nx::vms::client::desktop
