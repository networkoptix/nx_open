// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "indirect_access_decorator_model.h"

#include <algorithm>

#include <QtCore/QCollator>

#include <core/resource/layout_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/html/html.h>

namespace {

static const qsizetype kMaxTooltipResourceLines = 10;

using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

QString indirectAccessTooltipRichText(const QnResourceList& providers)
{
    if (providers.empty())
        return {};

    QStringList providerNames;
    for (const auto& provider: providers)
    {
        if (provider->hasFlags(Qn::layout)
            && provider.staticCast<QnLayoutResource>()->isServiceLayout())
        {
            continue;
        }
        providerNames.push_back(provider->getName());
    }
    providerNames.removeDuplicates();

    QCollator collator;
    collator.setNumericMode(true);
    std::sort(providerNames.begin(), providerNames.end(),
        [collator = std::move(collator)](const QString& lhs, const QString& rhs)
        {
            return collator.compare(lhs, rhs) < 0;
        });

    QStringList tooltipLines;
    tooltipLines.push_back(IndirectAccessDecoratorModel::tr("Access granted by:"));

    for (int i = 0; i < std::min(kMaxTooltipResourceLines, providerNames.size()); ++i)
        tooltipLines.push_back(html::bold(providerNames.at(i)));

    if (providerNames.size() > kMaxTooltipResourceLines)
    {
        tooltipLines.push_back(IndirectAccessDecoratorModel::tr(
            "and %n more", "", providerNames.size() - kMaxTooltipResourceLines));
    }

    return tooltipLines.join("<br>");
}

} // namespace

namespace nx::vms::client::desktop {

IndirectAccessDecoratorModel::IndirectAccessDecoratorModel(
    nx::core::access::ResourceAccessProvider* resourceAccessProvider,
    int indirectAccessIconColumn,
    QObject* parent)
    :
    base_type(parent),
    m_resourceAccessProvider(resourceAccessProvider),
    m_indirectAccessIconColumn(indirectAccessIconColumn)
{
    NX_ASSERT(m_resourceAccessProvider);
}

void IndirectAccessDecoratorModel::setSubject(const QnResourceAccessSubject& subject)
{
    if (m_subject == subject)
        return;

    m_subject = subject;

    if (rowCount() > 0)
    {
        emit dataChanged(
            index(0, m_indirectAccessIconColumn),
            index(rowCount() - 1, m_indirectAccessIconColumn));
    }
}

QVariant IndirectAccessDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return base_type::data(index, role);

    const auto sourceIndex = mapToSource(index);
    const auto sourceResourceIndex = sourceIndex.siblingAtColumn(
        accessible_media_selection_view::ResourceColumn);

    if (index.column() != m_indirectAccessIconColumn)
    {
        if (m_accessAllMedia)
        {
            if (role == Qt::CheckStateRole && !sourceIndex.data(Qt::CheckStateRole).isNull())
                return Qt::Checked;

            if (role == ResourceDialogItemRole::IsItemHighlightedRole)
                return true;
        }
        return base_type::data(index, role);
    }

    if (!m_resourceAccessProvider || !m_subject.isValid())
        return base_type::data(index, role);

    if (const auto resource = sourceResourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>())
    {
        if (role == Qt::DecorationRole)
        {
            const auto accessLevels =
                m_resourceAccessProvider->accessLevels(m_subject, resource);

            if (accessLevels.contains(nx::core::access::Source::layout))
                return qnResIconCache->icon(QnResourceIconCache::SharedLayout);

            if (accessLevels.contains(nx::core::access::Source::videowall))
                return qnResIconCache->icon(QnResourceIconCache::VideoWall);
        }

        if (role == Qt::ToolTipRole)
        {
            QnResourceList providers;
            m_resourceAccessProvider->accessibleVia(m_subject, resource, &providers);
            return indirectAccessTooltipRichText(providers);
        }
    }

    return base_type::data(index, role);
}

Qt::ItemFlags IndirectAccessDecoratorModel::flags(const QModelIndex& index) const
{
    auto result = base_type::flags(index);

    if (m_accessAllMedia)
        result.setFlag(Qt::ItemIsEnabled, false);

    return result;
}

void IndirectAccessDecoratorModel::setAccessAllMedia(bool value)
{
    if (m_accessAllMedia == value)
        return;

    m_accessAllMedia = value;

    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

} // namespace nx::vms::client::desktop
