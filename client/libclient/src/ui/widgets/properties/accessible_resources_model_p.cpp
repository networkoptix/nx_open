#include "accessible_resources_model_p.h"

#include <set>

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>

#include <nx/utils/string.h>

#include <ui/models/resource/resource_list_model.h>
#include <ui/style/resource_icon_cache.h>

namespace {

    const int kMaxResourcesInTooltip = 10;

} // anonymous namespace


QnAccessibleResourcesModel::QnAccessibleResourcesModel(QObject* parent) :
    base_type(parent),
    m_allChecked(false),
    m_indirectlyAccessibleDirty(true)
{
}

QnAccessibleResourcesModel::~QnAccessibleResourcesModel()
{
}

QnAccessibleResourcesModel::IndirectAccessFunction QnAccessibleResourcesModel::indirectAccessFunction() const
{
    return m_indirectAccessFunction;
}

void QnAccessibleResourcesModel::setIndirectAccessFunction(IndirectAccessFunction function)
{
    m_indirectAccessFunction = function;

    indirectAccessChanged();

    emit dataChanged(
        index(0, IndirectAccessColumn),
        index(rowCount() - 1, IndirectAccessColumn));
}

int QnAccessibleResourcesModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

QModelIndex QnAccessibleResourcesModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid())
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex QnAccessibleResourcesModel::sibling(int row, int column, const QModelIndex& idx) const
{
    return index(row, column, idx.parent());
}

QModelIndex QnAccessibleResourcesModel::parent(const QModelIndex& index) const
{
    Q_UNUSED(index);
    return QModelIndex();
}

QModelIndex QnAccessibleResourcesModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return QModelIndex();

    NX_ASSERT(proxyIndex.model() == this);
    switch (proxyIndex.column())
    {
        case NameColumn:
            return sourceModel()->index(proxyIndex.row(), QnResourceListModel::NameColumn);
        case CheckColumn:
            return sourceModel()->index(proxyIndex.row(), QnResourceListModel::CheckColumn);
        default:
            return QModelIndex();
    }
}

QModelIndex QnAccessibleResourcesModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();

    NX_ASSERT(sourceIndex.model() == sourceModel());
    switch (sourceIndex.column())
    {
        case QnResourceListModel::NameColumn:
            return createIndex(sourceIndex.row(), NameColumn);
        case QnResourceListModel::CheckColumn:
            return createIndex(sourceIndex.row(), CheckColumn);
        default:
            return QModelIndex();
    }
}

QnAccessibleResourcesModel::IndirectAccessInfo QnAccessibleResourcesModel::indirectAccessInfo(const QnResourcePtr& resource) const
{
    if (m_indirectlyAccessibleDirty)
    {
        m_indirectlyAccessibleResources.clear();
        if (m_indirectAccessFunction)
            m_indirectlyAccessibleResources = m_indirectAccessFunction();

        m_indirectlyAccessibleDirty = false;
    }

    if (m_indirectlyAccessibleResources.empty())
        return IndirectAccessInfo();

    auto info = m_indirectAccessInfoCache.find(resource);
    if (info != m_indirectAccessInfoCache.end())
        return info.value();

    auto providers = m_indirectlyAccessibleResources[resource->getId()];
    std::set<QString, decltype(&nx::utils::naturalStringLess)> names(&nx::utils::naturalStringLess);
    QIcon icon;

    for (auto provider : providers)
    {
        /* Get first normal icon: */
        if (icon.isNull())
            icon = qnResIconCache->icon(provider);

        /* Sort all names: */
        names.insert(provider->getName());
    }

    int count = 0;
    QString tooltip;

    /* Show only first kMaxResourcesInTooltip names from the sorted list: */
    for (const auto& name : names)
    {
        if (++count > kMaxResourcesInTooltip)
            break;

        tooltip += lit("<br>&nbsp;&nbsp;&nbsp;%1").arg(name);
    }

    if (!tooltip.isEmpty())
    {
        QString suffix;
        if (count > kMaxResourcesInTooltip)
            suffix = lit("<br>&nbsp;&nbsp;&nbsp;%1").arg(tr("...and %n more", "", count - kMaxResourcesInTooltip));

        tooltip = lit("<div style='white-space: pre'>%1<b>%2</b>%3</div>").
            arg(tr("Access granted by:")).
            arg(tooltip).
            arg(suffix);
    }

    info = m_indirectAccessInfoCache.insert(resource, IndirectAccessInfo(icon, tooltip));
    return info.value();
}

QVariant QnAccessibleResourcesModel::data(const QModelIndex& index, int role) const
{
    switch (index.column())
    {
        case IndirectAccessColumn:
        {
            switch (role)
            {
                case Qt::DecorationRole:
                    return indirectAccessInfo(index.sibling(index.row(), NameColumn).
                        data(Qn::ResourceRole).value<QnResourcePtr>()).first;

                case Qt::ToolTipRole:
                    return indirectAccessInfo(index.sibling(index.row(), NameColumn).
                        data(Qn::ResourceRole).value<QnResourcePtr>()).second;

                case Qn::DisabledRole:
                    return index.sibling(index.row(), NameColumn).data(role);

                default:
                    return QVariant();
            }
        }

        case CheckColumn:
        {
            if (m_allChecked && role == Qt::CheckStateRole)
                return QVariant::fromValue<int>(Qt::Checked);

            break;
        }

        default:
            break;
    }

    return base_type::data(index, role);
}

Qt::ItemFlags QnAccessibleResourcesModel::flags(const QModelIndex& index) const
{
    if (index.column() == IndirectAccessColumn)
        return base_type::flags(index.sibling(index.row(), NameColumn));

    return base_type::flags(index);
}

bool QnAccessibleResourcesModel::allChecked() const
{
    return m_allChecked;
}

void QnAccessibleResourcesModel::setAllChecked(bool value)
{
    if (m_allChecked == value)
        return;

    m_allChecked = value;

    emit dataChanged(
        index(0, CheckColumn),
        index(rowCount() - 1, CheckColumn),
        { Qt::CheckStateRole });
}

void QnAccessibleResourcesModel::indirectAccessChanged()
{
    m_indirectAccessInfoCache.clear();
    m_indirectlyAccessibleDirty = true;

    emit dataChanged(index(0, IndirectAccessColumn), index(rowCount() - 1, IndirectAccessColumn));
}


